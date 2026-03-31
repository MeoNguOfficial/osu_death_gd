#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// --- State toàn cục ---
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;

class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);
        if (!s_isDeadEffect) return;

        // 1. ÉP PITCH MASTER (SFX)
        if (m_system) {
            FMOD::ChannelGroup* masterGroup = nullptr;
            m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) masterGroup->setPitch(s_targetPitch);
        }

        // 2. TRỊ MUSIC (Cưỡng chế tầng sâu)
        if (m_backgroundMusicChannel) {
            // Ép Volume và Pitch trực tiếp vào Channel
            m_backgroundMusicChannel->setVolume(this->m_musicVolume);
            m_backgroundMusicChannel->setPitch(s_targetPitch);

            // TÌM GROUP: Nếu channel bị ngắt, ta tìm Group của nó để "kéo" lại
            FMOD::ChannelGroup* musicGroup = nullptr;
            m_backgroundMusicChannel->getChannelGroup(&musicGroup);
            if (musicGroup) {
                musicGroup->setPitch(s_targetPitch);
                musicGroup->setVolume(this->m_musicVolume);
                musicGroup->setPaused(false); // Ép Group phải phát
            }

            // CHỐNG STOP: Ép channel không được ở trạng thái Pause
            bool isPaused = false;
            m_backgroundMusicChannel->getPaused(&isPaused);
            if (isPaused && s_targetPitch > 0.05f) {
                m_backgroundMusicChannel->setPaused(false);
            }
        }
    }

    // Chặn lệnh dừng nhạc từ GD
    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return;
        FMODAudioEngine::stopAllMusic(p0);
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    void onPlayerReallyDied() {
        if (s_isDeadEffect) return;
        s_isDeadEffect = true;
        s_targetPitch = 1.0f;
        
        auto speed = Mod::get()->getSettingValue<double>("fade-speed");
        s_duration = static_cast<float>(speed);

        // Kích hoạt ngay lập tức
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPaused(false);
            fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
        }
        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));
    }

    void updateOsuFade(float dt) {
        if (!s_isDeadEffect) return;
        s_targetPitch -= (dt / s_duration);

        if (s_targetPitch <= 0.01f) {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));
            
            if (auto fmod = FMODAudioEngine::sharedEngine()) {
                if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPaused(true);
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
                FMOD::ChannelGroup* mg = nullptr;
                fmod->m_system->getMasterChannelGroup(&mg);
                if (mg) mg->setPitch(1.0f);
            }
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }
        }
    }

    void resetLevel() { resetOsuVars(); PlayLayer::resetLevel(); }
    void onQuit() { resetOsuVars(); PlayLayer::onQuit(); }
};

class $modify(MyPlayerObject, PlayerObject) {
    void playDeathEffect() {
        PlayerObject::playDeathEffect();
        if (auto pl = PlayLayer::get()) {
            static_cast<MyPlayLayer*>(pl)->onPlayerReallyDied();
        }
    }
};