#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <chrono>

using namespace geode::prelude;

// --- State toàn cục ---
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;
static std::chrono::steady_clock::time_point s_startTime;

class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);

        if (!s_isDeadEffect) return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_startTime).count() / 1000.0f;
        
        s_targetPitch = 1.0f - (elapsed / s_duration);
        if (s_targetPitch < 0.01f) s_targetPitch = 0.01f;

        // 1. ÉP PITCH TOÀN CỤC (Đánh vào Global Channel - Trùm cuối bản 2.2081)
        if (m_globalChannel) {
            m_globalChannel->setPitch(s_targetPitch);
        }

        // 2. ÉP MASTER GROUP (Dành cho SFX)
        if (m_system) {
            FMOD::ChannelGroup* masterGroup = nullptr;
            m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) masterGroup->setPitch(s_targetPitch);
        }

        // 3. ÉP MUSIC (Nếu nó chưa bị vứt vào stoppedChannels)
        if (m_backgroundMusicChannel) {
            m_backgroundMusicChannel->setPitch(s_targetPitch);
            m_backgroundMusicChannel->setVolume(this->m_musicVolume);
            
            // Cưỡng bách phát lại nếu bị GD tự ý Pause
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
    // FIX CỘNG HƯỞNG: Reset mọi thứ khi bắt đầu level mới
    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        this->resetOsuVars();
        return true;
    }

    void onPlayerReallyDied() {
        if (s_isDeadEffect) return;
        s_isDeadEffect = true;
        s_targetPitch = 1.0f;
        s_startTime = std::chrono::steady_clock::now();

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        s_duration = static_cast<float>(speedValue);

        // Resume nhạc ngay lập tức để chống "câm"
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPaused(false);
        }

        this->schedule(schedule_selector(MyPlayLayer::checkEffectEnd));
    }

    void checkEffectEnd(float dt) {
        if (s_targetPitch <= 0.01f) {
            s_isDeadEffect = false;
            this->unschedule(schedule_selector(MyPlayLayer::checkEffectEnd));
        }
    }

    void resetOsuVars() {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        this->unschedule(schedule_selector(MyPlayLayer::checkEffectEnd));

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            // Trả lại Pitch cho Master
            if (fmod->m_system) {
                FMOD::ChannelGroup* mg = nullptr;
                fmod->m_system->getMasterChannelGroup(&mg);
                if (mg) mg->setPitch(1.0f);
            }
            // Trả lại Pitch cho Global (Quan trọng!)
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(1.0f);
            }
            // Trả lại Music
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }
        }
    }

    void resetLevel() { this->resetOsuVars(); PlayLayer::resetLevel(); }
    void onQuit() { this->resetOsuVars(); PlayLayer::onQuit(); }
};

class $modify(MyPlayerObject, PlayerObject) {
    void playDeathEffect() {
        PlayerObject::playDeathEffect();
        if (auto pl = PlayLayer::get()) {
            static_cast<MyPlayLayer*>(pl)->onPlayerReallyDied();
        }
    }
};