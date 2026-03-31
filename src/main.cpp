#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// --- Trạng thái toàn cục (Global State) ---
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;

// --- 1. Can thiệp trực tiếp vào Engine âm thanh (FMOD) ---
class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    
    // Khóa Volume: Ngăn GD ép âm lượng Music về 0 khi chết
    void setBackgroundMusicVolume(float volume) {
        if (s_isDeadEffect) {
            // Ép volume giữ nguyên theo mức người dùng cài trong settings
            FMODAudioEngine::setBackgroundMusicVolume(this->m_musicVolume);
            return;
        }
        FMODAudioEngine::setBackgroundMusicVolume(volume);
    }

    void update(float dt) {
        FMODAudioEngine::update(dt);

        if (!s_isDeadEffect) return;

        // Tác động vào Master Group: Pitch toàn bộ âm thanh (SFX + Music) cho đồng bộ
        if (m_system) {
            FMOD::ChannelGroup* masterGroup = nullptr;
            m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) {
                masterGroup->setPitch(s_targetPitch);
            }
        }

        // Ép Music Channel: Đảm bảo nhạc vẫn chạy và bị bẻ pitch
        if (m_backgroundMusicChannel) {
            m_backgroundMusicChannel->setPitch(s_targetPitch);

            bool isPaused = false;
            m_backgroundMusicChannel->getPaused(&isPaused);
            // Nếu pitch chưa về mức quá thấp, không cho phép nhạc bị dừng
            if (isPaused && s_targetPitch > 0.05f) {
                m_backgroundMusicChannel->setPaused(false);
            }
        }
    }

    // Chặn lệnh Stop: Không cho game ngắt luồng nhạc khi đang chạy hiệu ứng
    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return;
        FMODAudioEngine::stopAllMusic(p0);
    }
};

// --- 2. Quản lý logic màn chơi (PlayLayer) ---
class $modify(MyPlayLayer, PlayLayer) {
    
    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        this->resetOsuVars();
        return true;
    }

    void onPlayerReallyDied() {
        if (s_isDeadEffect) return;

        s_isDeadEffect = true;
        s_targetPitch = 1.0f;

        // Đọc giá trị từ file mod.json bạn vừa gửi
        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        s_duration = static_cast<float>(speedValue);
        if (s_duration < 0.1f) s_duration = 1.0f;

        // Bắt đầu vòng lặp giảm pitch (Fade)
        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));
        log::info("Osu Death Effect started - Duration: {}s", s_duration);
    }

    void updateOsuFade(float dt) {
        if (!s_isDeadEffect) return;

        // Giảm tuyến tính theo thời gian thực (dt) để mượt trên mọi FPS
        s_targetPitch -= (dt / s_duration);

        if (s_targetPitch <= 0.01f) {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

            // Chỉ sau khi hoàn tất hiệu ứng mới cho nhạc dừng hẳn
            if (auto fmod = FMODAudioEngine::sharedEngine()) {
                if (fmod->m_backgroundMusicChannel)
                    fmod->m_backgroundMusicChannel->setPaused(true);
            }
            log::info("Osu Death Effect finished");
        }
    }

    // Hàm trả lại trạng thái bình thường (Reset)
    void resetOsuVars() {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            // Trả pitch Master về 1.0
            if (fmod->m_system) {
                FMOD::ChannelGroup* masterGroup = nullptr;
                fmod->m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup) masterGroup->setPitch(1.0f);
            }
            // Trả pitch Music về 1.0 và bật lại volume chuẩn
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }
            if (fmod->m_globalChannel)
                fmod->m_globalChannel->setPitch(1.0f);
        }
    }

    void resetLevel() {
        this->resetOsuVars();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        this->resetOsuVars();
        PlayLayer::onQuit();
    }
};

// --- 3. Điểm kích hoạt (PlayerObject) ---
class $modify(MyPlayerObject, PlayerObject) {
    void playDeathEffect() {
        PlayerObject::playDeathEffect();
        
        // Gọi lệnh kích hoạt sang PlayLayer
        if (auto pl = PlayLayer::get()) {
            static_cast<MyPlayLayer*>(pl)->onPlayerReallyDied();
        }
    }
};