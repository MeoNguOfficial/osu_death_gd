#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// --- State điều khiển ---
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;

class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    void update(float dt) {
        // Nếu đang trong hiệu ứng chết, ta kiểm soát dt để khớp Speedhack
        // dt ở đây đã bao gồm tốc độ của Speedhack/TimeWarp
        if (s_isDeadEffect) {
            s_targetPitch -= (dt / s_duration);
            if (s_targetPitch < 0.01f) s_targetPitch = 0.01f;

            // 1. ÉP PITCH MASTER (SFX)
            if (m_system) {
                FMOD::ChannelGroup* masterGroup = nullptr;
                m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup) masterGroup->setPitch(s_targetPitch);
            }

            // 2. ÉP MUSIC (Dùng Global Channel để bao quát cả Music Channel)
            if (m_globalChannel) {
                m_globalChannel->setPitch(s_targetPitch);
            }

            if (m_backgroundMusicChannel) {
                m_backgroundMusicChannel->setPitch(s_targetPitch);
                // Khóa Volume theo settings của Mèo, chặn GD tự fade-out về 0
                m_backgroundMusicChannel->setVolume(this->m_musicVolume);
                
                bool isPaused = false;
                m_backgroundMusicChannel->getPaused(&isPaused);
                if (isPaused && s_targetPitch > 0.1f) {
                    m_backgroundMusicChannel->setPaused(false);
                }
            }
        }

        FMODAudioEngine::update(dt);
    }

    // CHẶN GD HỦY NHẠC: Đây là lý do Music của Mèo không có tác dụng
    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return; 
        FMODAudioEngine::stopAllMusic(p0);
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    // FIX CỘNG HƯỞNG: Đảm bảo Pitch về 1.0 ngay khi bắt đầu Level mới hoặc Reset
    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
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

        // Resume nhạc ngay lập tức
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPaused(false);
        }
    }

    void resetOsuVars() {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            // Trả lại Pitch cho Master và Global
            if (fmod->m_system) {
                FMOD::ChannelGroup* mg = nullptr;
                fmod->m_system->getMasterChannelGroup(&mg);
                if (mg) mg->setPitch(1.0f);
            }
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);
            
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }
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

class $modify(MyPlayerObject, PlayerObject) {
    void playDeathEffect() {
        PlayerObject::playDeathEffect();
        if (auto pl = PlayLayer::get()) {
            static_cast<MyPlayLayer*>(pl)->onPlayerReallyDied();
        }
    }
};