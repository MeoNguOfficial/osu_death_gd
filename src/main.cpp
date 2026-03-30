#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    static inline float s_customPitch = 1.0f;
    static inline bool s_isFading = false;
    
    void update(float dt) {
        FMODAudioEngine::update(dt);
        
        // Apply custom pitch nếu đang fade
        if (s_isFading && s_customPitch != 1.0f) {
            if (this->m_backgroundMusicChannel) {
                this->m_backgroundMusicChannel->setPitch(s_customPitch);
            }
            if (this->m_globalChannel) {
                this->m_globalChannel->setPitch(s_customPitch);
            }
        }
    }
    
    static void setCustomPitch(float pitch, bool isFading = true) {
        s_customPitch = pitch;
        s_isFading = isFading;
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(pitch);
            }
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(pitch);
            }
        }
    }
    
    static void stopFade() {
        s_isFading = false;
        s_customPitch = 1.0f;
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
            }
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(1.0f);
            }
        }
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        bool m_hasTriggeredDeath = false;
        float m_time = 0.0f;
        int m_actionTag = 1001;
        bool m_hasStarted = false;
        bool m_wasPlayerDead = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        m_fields->m_hasTriggeredDeath = false;
        m_fields->m_time = 0.0f;
        m_fields->m_hasStarted = true;
        m_fields->m_wasPlayerDead = false;
        
        // Chỉ reset khi init level
        MyFMODAudioEngine::stopFade();
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        
        // Chỉ kiểm tra khi chưa trigger death
        if (m_fields->m_hasStarted && !m_fields->m_hasTriggeredDeath) {
            bool isPlayerDead = false;
            
            if (this->m_player1 && this->m_player1->m_isDead) {
                isPlayerDead = true;
            }
            if (this->m_player2 && this->m_player2->m_isDead) {
                isPlayerDead = true;
            }
            
            if (isPlayerDead && !m_fields->m_wasPlayerDead) {
                this->triggerDeathEffect();
            }
            
            m_fields->m_wasPlayerDead = isPlayerDead;
        }
    }

    void triggerDeathEffect() {
        if (!m_fields->m_hasTriggeredDeath) {
            m_fields->m_hasTriggeredDeath = true;
            m_fields->m_time = 0.0f;

            this->stopActionByTag(m_fields->m_actionTag);

            // Bắt đầu fade
            auto delay = CCDelayTime::create(1.0f / 60.0f);
            auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::applyOsuPitch));
            auto seq = CCSequence::create(delay, call, nullptr);
            auto repeat = CCRepeatForever::create(seq);
            repeat->setTag(m_fields->m_actionTag);
            
            this->runAction(repeat);
            log::info("Osu Death triggered - Starting pitch fade!");
        }
    }

    void applyOsuPitch() {
        if (!m_fields->m_hasTriggeredDeath) {
            return;
        }
        
        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = static_cast<float>(speedValue);
        if (duration <= 0.01f) duration = 1.0f;

        m_fields->m_time += 1.0f / 60.0f;
        float progress = 1.0f - (m_fields->m_time / duration);

        if (progress <= 0.0f) {
            progress = 0.0f;
            // Kết thúc fade, dừng action
            this->stopActionByTag(m_fields->m_actionTag);
            MyFMODAudioEngine::setCustomPitch(0.0f, false);
            log::info("Fade complete - Pitch at 0");
            return;
        }

        float finalPitch = progress * progress;
        MyFMODAudioEngine::setCustomPitch(finalPitch, true);
        log::info("Fading - Pitch: {}", finalPitch);
    }

    void resetLevel() {
        // Reset level nhưng không reset pitch ngay
        PlayLayer::resetLevel();
        
        // Reset flags
        m_fields->m_hasTriggeredDeath = false;
        m_fields->m_wasPlayerDead = false;
        m_fields->m_time = 0.0f;
        
        // Dừng fade và reset pitch
        this->stopActionByTag(m_fields->m_actionTag);
        MyFMODAudioEngine::stopFade();
        log::info("Level reset - Pitch reset to 1.0");
    }

    void onQuit() {
        this->stopActionByTag(m_fields->m_actionTag);
        MyFMODAudioEngine::stopFade();
        PlayLayer::onQuit();
    }
};