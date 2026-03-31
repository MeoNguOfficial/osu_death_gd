#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;

class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);
        if (!s_isDeadEffect) return;

        // ÉP PITCH QUA MASTER GROUP (Để SFX chạy được)
        if (m_system) {
            FMOD::ChannelGroup* masterGroup = nullptr;
            m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) {
                masterGroup->setPitch(s_targetPitch);
            }
        }

        // ÉP NHẠC (Chỉ Pitch và Volume, không gọi Resume liên tục để tránh mất tiếng)
        if (m_backgroundMusicChannel) {
            m_backgroundMusicChannel->setPitch(s_targetPitch);
            m_backgroundMusicChannel->setVolume(this->m_musicVolume);
        }
    }

    // CHẶN TUYỆT ĐỐI LỆNH STOP (Đây là lý do mất tiếng)
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

        // KÍCH HOẠT NHẠC NGAY LÚC CHẾT
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            // Ép nhạc chạy lại 1 lần duy nhất lúc này
            fmod->m_backgroundMusicChannel->setPaused(false);
            fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
        }

        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));
    }

    void updateOsuFade(float dt) {
        if (!s_isDeadEffect) return;
        s_targetPitch -= (dt / s_duration);

        if (s_targetPitch <= 0.05f) {
            s_targetPitch = 0.05f;
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
        
        if (auto fmod = FMODAudioEngine::sharedEngine()) {
            // Trả lại Pitch Master
            if (fmod->m_system) {
                FMOD::ChannelGroup* mg = nullptr;
                fmod->m_system->getMasterChannelGroup(&mg);
                if (mg) mg->setPitch(1.0f);
            }
            // Trả lại Music
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

// Hook vào PlayerObject để chắc chắn hiệu ứng gọi đúng lúc
#include <Geode/modify/PlayerObject.hpp>
class $modify(PlayerObject) {
    void playDeathEffect() {
        PlayerObject::playDeathEffect();
        if (auto pl = PlayLayer::get()) {
            static_cast<MyPlayLayer*>(pl)->onPlayerReallyDied();
        }
    }
};