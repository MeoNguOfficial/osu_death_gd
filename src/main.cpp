#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// --- Global State ---
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;

// --- 1. Hook FMOD: Bỏ cái hàm setBackgroundMusicVolume bị lỗi đi ---
class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    
    void update(float dt) {
        FMODAudioEngine::update(dt);

        if (!s_isDeadEffect) return;

        // ÉP VOLUME TRONG UPDATE: 
        // Vì không hook được hàm set volume, ta sẽ ép volume liên tục ở đây.
        // GD có gọi set volume về 0 thì frame sau mình lại ép nó lên lại.
        if (m_backgroundMusicChannel) {
            // Ép volume về mức music settings của game
            m_backgroundMusicChannel->setVolume(this->m_musicVolume);
            
            // Ép Pitch
            m_backgroundMusicChannel->setPitch(s_targetPitch);

            // Đảm bảo không bị Pause
            bool isPaused = false;
            m_backgroundMusicChannel->getPaused(&isPaused);
            if (isPaused && s_targetPitch > 0.05f) {
                m_backgroundMusicChannel->setPaused(false);
            }
        }

        // Master Group cho SFX trầm xuống
        if (m_system) {
            FMOD::ChannelGroup* masterGroup = nullptr;
            m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) {
                masterGroup->setPitch(s_targetPitch);
            }
        }
    }

    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return;
        FMODAudioEngine::stopAllMusic(p0);
    }
};

// --- 2. Hook PlayLayer: Giữ nguyên logic cũ ---
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

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        s_duration = static_cast<float>(speedValue);
        if (s_duration < 0.1f) s_duration = 1.0f;

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

// --- 3. Hook PlayerObject ---
class $modify(MyPlayerObject, PlayerObject) {
    void playDeathEffect() {
        PlayerObject::playDeathEffect();
        if (auto pl = PlayLayer::get()) {
            static_cast<MyPlayLayer*>(pl)->onPlayerReallyDied();
        }
    }
};