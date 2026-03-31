#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// --- Trạng thái toàn cục ---
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;

// --- 1. Hook FMOD: Ép nhạc phải phát ---
class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);

        if (!s_isDeadEffect) return;

        // ÉP PITCH MASTER (Dành cho SFX và toàn bộ âm thanh game)
        if (m_system) {
            FMOD::ChannelGroup* masterGroup = nullptr;
            m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) {
                masterGroup->setPitch(s_targetPitch);
            }
        }

        // ÉP MUSIC: Chiến thuật cưỡng chế cho 2.2081
        if (m_backgroundMusicChannel) {
            // 1. Ép Pitch riêng cho Music
            m_backgroundMusicChannel->setPitch(s_targetPitch);
            
            // 2. Ép Volume (Quan trọng: dùng trực tiếp m_musicVolume của engine)
            m_backgroundMusicChannel->setVolume(this->m_musicVolume);

            // 3. Chống Pause: Nếu GD cố tình pause nhạc khi chết, ta resume lại ngay
            bool isPaused = false;
            m_backgroundMusicChannel->getPaused(&isPaused);
            if (isPaused && s_targetPitch > 0.05f) {
                m_backgroundMusicChannel->setPaused(false);
            }
        }
    }

    // Chặn lệnh dừng nhạc hoàn toàn
    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return;
        FMODAudioEngine::stopAllMusic(p0);
    }
};

// --- 2. Hook PlayLayer: Quản lý Fade Timeline ---
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

        // Lấy speed từ settings mod.json của Mèo
        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        s_duration = static_cast<float>(speedValue);
        if (s_duration < 0.1f) s_duration = 1.0f;

        // Kích hoạt Resume nhạc ngay lập tức lúc vừa chạm gai
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPaused(false);
        }

        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));
    }

    void updateOsuFade(float dt) {
        if (!s_isDeadEffect) return;

        // Giảm tuyến tính mượt mà
        s_targetPitch -= (dt / s_duration);

        if (s_targetPitch <= 0.01f) {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

            // Hiệu ứng xong rồi mới cho phép nhạc dừng
            if (auto fmod = FMODAudioEngine::sharedEngine()) {
                if (fmod->m_backgroundMusicChannel)
                    fmod->m_backgroundMusicChannel->setPaused(true);
            }
        }
    }

    void resetOsuVars() {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_system) {
                FMOD::ChannelGroup* masterGroup = nullptr;
                fmod->m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup) masterGroup->setPitch(1.0f);
            }
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }
        }
    }

    void resetLevel() { this->resetOsuVars(); PlayLayer::resetLevel(); }
    void onQuit() { this->resetOsuVars(); PlayLayer::onQuit(); }
};

// --- 3. Hook PlayerObject: Kích hoạt ---
class $modify(MyPlayerObject, PlayerObject) {
    void playDeathEffect() {
        PlayerObject::playDeathEffect();
        if (auto pl = PlayLayer::get()) {
            static_cast<MyPlayLayer*>(pl)->onPlayerReallyDied();
        }
    }
};