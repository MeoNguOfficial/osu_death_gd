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
            // --- ĐOẠN THAY THẾ/BỔ SUNG Ở ĐÂY ---
            if (m_system)
            {
                FMOD::ChannelGroup *masterGroup;
                // Lấy group tổng quản lý toàn bộ sound/music
                m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup)
                {
                    // Ép pitch toàn bộ âm thanh game theo biến s_targetPitch
                    masterGroup->setPitch(s_targetPitch);
                }
            }
            // ------------------------------------

            // Bạn vẫn nên giữ đoạn ép Resume cho Music để chắc chắn nhạc không dừng
            if (m_backgroundMusicChannel)
            {
                m_backgroundMusicChannel->setVolume(this->m_musicVolume);
                bool isPaused;
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
            return;
        FMODAudioEngine::stopAllMusic(p0);
    }
};

// 2. Hook PlayLayer: Quản lý logic thời gian (Timeline) của cú ngã
class $modify(MyPlayLayer, PlayLayer)
{
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

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = (speedValue <= 0.05) ? 1.0f : static_cast<float>(speedValue);

        // Bắt đầu vòng lặp giảm pitch mỗi frame
        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));
    }

    void updateOsuFade(float dt)
    {
        if (!s_isDeadEffect)
            return;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = (speedValue <= 0.05) ? 1.0f : static_cast<float>(speedValue);

        // Giảm pitch dựa trên thời gian thực (dt) giúp hiệu ứng mượt ở mọi mức FPS
        s_targetPitch -= (dt / duration);

        if (s_targetPitch <= 0.01f)
        {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

            // Hiệu ứng kết thúc, bây giờ mới thực sự cho nhạc dừng
            if (auto fmod = FMODAudioEngine::sharedEngine())
            {
                if (fmod->m_backgroundMusicChannel)
                    fmod->m_backgroundMusicChannel->setPaused(true);
            }
        }
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

        // Dừng việc giảm pitch đang chạy ngầm
        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        if (auto fmod = FMODAudioEngine::sharedEngine())
        {
            // 1. Trả lại Pitch cho Master Group (Vì chúng ta đã can thiệp vào nó)
            if (fmod->m_system)
            {
                FMOD::ChannelGroup *masterGroup;
                fmod->m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup)
                {
                    masterGroup->setPitch(1.0f);
                }
            }

            // 2. Trả lại trạng thái cho Music Channel
            if (fmod->m_backgroundMusicChannel)
            {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
                fmod->m_backgroundMusicChannel->setPaused(false); // Đảm bảo nhạc chạy lại
            }

            // 3. Trả lại Pitch cho SFX
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

        if (auto playLayer = PlayLayer::get())
        {
            // Sử dụng static_cast để gọi hàm xử lý âm thanh
            static_cast<MyPlayLayer *>(playLayer)->onPlayerReallyDied();
        }
    }

    // Đảm bảo không có code thừa đè lên s_isDeadEffect ở đây
    void playerDestroyed(bool p0)
    {
        PlayerObject::playerDestroyed(p0);
    }
};