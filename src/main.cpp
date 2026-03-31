#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Các biến điều khiển trạng thái hiệu ứng
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;

// 1. Hook FMOD: Nơi xử lý âm thanh tổng thể
class $modify(MyFMOD, FMODAudioEngine)
{
    void update(float dt)
    {
        // Chạy logic gốc trước
        FMODAudioEngine::update(dt);

        if (s_isDeadEffect)
        {
            // --- ÉP PITCH QUA MASTER CHANNEL GROUP ---
            if (m_system)
            {
                FMOD::ChannelGroup* masterGroup = nullptr;
                // Lấy group tổng quản lý toàn bộ sound/music
                FMOD_RESULT result = m_system->getMasterChannelGroup(&masterGroup);
                if (result == FMOD_OK && masterGroup)
                {
                    // Ép pitch toàn bộ âm thanh game
                    masterGroup->setPitch(s_targetPitch);
                    
                    // Debug: in ra pitch hiện tại (có thể bỏ comment nếu cần debug)
                    // float currentPitch;
                    // masterGroup->getPitch(&currentPitch);
                    // log::debug("Master group pitch: {}", currentPitch);
                }
                else
                {
                    log::error("Failed to get master channel group, result: {}", result);
                }
            }
            
            // Đảm bảo music không bị pause
            if (m_backgroundMusicChannel)
            {
                bool isPaused = false;
                m_backgroundMusicChannel->getPaused(&isPaused);
                if (isPaused && s_targetPitch > 0.05f)
                {
                    m_backgroundMusicChannel->setPaused(false);
                }
            }
        }
    }

    void stopAllMusic(bool p0)
    {
        if (s_isDeadEffect)
        {
            // Không cho stop music khi đang trong effect
            return;
        }
        FMODAudioEngine::stopAllMusic(p0);
    }
    
    // Thêm hook pauseBackgroundMusic để đảm bảo không bị gián đoạn
    void pauseBackgroundMusic()
    {
        if (s_isDeadEffect)
        {
            return;
        }
        FMODAudioEngine::pauseBackgroundMusic();
    }
};

// 2. Hook PlayLayer: Quản lý logic thời gian (Timeline) của cú ngã
class $modify(MyPlayLayer, PlayLayer)
{
    struct Fields
    {
        bool m_isFading = false;
    };
    
    bool init(GJGameLevel *level, bool useReplay, bool dontRun)
    {
        if (!PlayLayer::init(level, useReplay, dontRun))
            return false;
        this->resetOsuVars();
        return true;
    }

    void onPlayerReallyDied()
    {
        if (s_isDeadEffect)
            return; // Tránh gọi trùng lặp

        s_isDeadEffect = true;
        s_targetPitch = 1.0f;
        
        // Lấy duration từ setting
        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = (speedValue <= 0.05f) ? 1.0f : static_cast<float>(speedValue);
        
        // Lưu duration vào fields để dùng trong update
        m_fields->m_isFading = true;

        // Bắt đầu vòng lặp giảm pitch mỗi frame
        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));
        
        log::info("Osu Death Effect started - Duration: {} seconds", duration);
    }

    void updateOsuFade(float dt)
    {
        if (!s_isDeadEffect)
        {
            // Nếu effect đã kết thúc, unschedule
            if (m_fields->m_isFading)
            {
                this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));
                m_fields->m_isFading = false;
            }
            return;
        }

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = (speedValue <= 0.05f) ? 1.0f : static_cast<float>(speedValue);
        
        // Giảm pitch dựa trên thời gian thực (dt)
        float decrement = dt / duration;
        s_targetPitch -= decrement;

        if (s_targetPitch <= 0.01f)
        {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;
            m_fields->m_isFading = false;
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

            // Hiệu ứng kết thúc, bây giờ mới thực sự cho nhạc dừng
            auto fmod = FMODAudioEngine::sharedEngine();
            if (fmod && fmod->m_backgroundMusicChannel)
            {
                fmod->m_backgroundMusicChannel->setPaused(true);
            }
            
            log::info("Osu Death Effect finished");
        }
        
        // Debug log mỗi 30 frame (có thể bỏ comment nếu cần)
        // static int frameCount = 0;
        // if (++frameCount % 30 == 0)
        // {
        //     log::debug("Target pitch: {}", s_targetPitch);
        // }
    }

    // Reset lại mọi thứ khi chơi lại hoặc thoát
    void resetLevel()
    {
        this->resetOsuVars();
        PlayLayer::resetLevel();
    }

    void onQuit()
    {
        this->resetOsuVars();
        PlayLayer::onQuit();
    }

    void resetOsuVars()
    {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        
        if (m_fields)
            m_fields->m_isFading = false;

        // Dừng việc giảm pitch đang chạy ngầm
        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod)
        {
            // 1. Trả lại Pitch cho Master Group
            if (fmod->m_system)
            {
                FMOD::ChannelGroup* masterGroup = nullptr;
                FMOD_RESULT result = fmod->m_system->getMasterChannelGroup(&masterGroup);
                if (result == FMOD_OK && masterGroup)
                {
                    masterGroup->setPitch(1.0f);
                }
            }

            // 2. Trả lại trạng thái cho Music Channel
            if (fmod->m_backgroundMusicChannel)
            {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }

            // 3. Trả lại Pitch cho SFX (để chắc chắn)
            if (fmod->m_globalChannel)
            {
                fmod->m_globalChannel->setPitch(1.0f);
            }
        }
    }
};

// 3. Hook PlayerObject: Điểm kích hoạt (Trigger)
class $modify(MyPlayerObject, PlayerObject)
{
    void playDeathEffect()
    {
        PlayerObject::playDeathEffect();

        auto playLayer = PlayLayer::get();
        if (playLayer)
        {
            // Gọi hàm xử lý âm thanh
            static_cast<MyPlayLayer*>(playLayer)->onPlayerReallyDied();
        }
    }

    void playerDestroyed(bool p0)
    {
        PlayerObject::playerDestroyed(p0);
        // Không cần làm gì thêm vì playDeathEffect đã trigger effect
    }
};