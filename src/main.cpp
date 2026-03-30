#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    static inline int priority = -99999;

    struct Fields {
        bool m_isDead = false;
        bool m_hasTriggeredDeath = false;
        float m_time = 0.0f;
        int m_actionTag = 1001;
        bool m_hasStarted = false;
        bool m_isFading = false;
        bool m_wasPlayerDead = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        m_fields->m_isDead = false;
        m_fields->m_hasTriggeredDeath = false;
        m_fields->m_time = 0.0f;
        m_fields->m_hasStarted = true;
        m_fields->m_isFading = false;
        m_fields->m_wasPlayerDead = false;
        
        this->cleanupOsuEffect();
        return true;
    }

    void gameUpdate(float dt) {
        PlayLayer::gameUpdate(dt);
        
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
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
            m_fields->m_isFading = true;

            this->stopActionByTag(m_fields->m_actionTag);

            auto delay = CCDelayTime::create(1.0f / 60.0f);
            auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::applyOsuPitch));
            auto seq = CCSequence::create(delay, call, nullptr);
            auto repeat = CCRepeatForever::create(seq);
            repeat->setTag(m_fields->m_actionTag);
            
            this->runAction(repeat);
            log::info("Osu Death triggered - Music and SFX will fade!");
        }
    }

    void applyOsuPitch() {
        if (!m_fields->m_isDead || !m_fields->m_isFading) {
            return;
        }

        auto fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;
        
        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = static_cast<float>(speedValue);
        if (duration <= 0.01f) duration = 1.0f;

        m_fields->m_time += 1.0f / 60.0f;
        float progress = 1.0f - (m_fields->m_time / duration);

        if (progress <= 0.0f) {
            progress = 0.0f;
            m_fields->m_isFading = false;
            this->stopActionByTag(m_fields->m_actionTag);
        }

        float finalPitch = progress * progress;
        
        // Set pitch cho BACKGROUND MUSIC
        if (fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPitch(finalPitch);
            log::info("Background music pitch: {}", finalPitch);
        }
        
        // Set pitch cho tất cả SFX channels
        if (fmod->m_globalChannel) {
            fmod->m_globalChannel->setPitch(finalPitch);
        }
        
        // Set pitch cho các channel khác nếu cần
        for (auto& channel : fmod->m_channels) {
            if (channel && channel != fmod->m_backgroundMusicChannel) {
                channel->setPitch(finalPitch);
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() {
        this->cleanupOsuEffect();
        PlayLayer::resetLevel();
        m_fields->m_isDead = false;
        m_fields->m_hasTriggeredDeath = false;
        m_fields->m_isFading = false;
        m_fields->m_wasPlayerDead = false;
        m_fields->m_time = 0.0f;
    }

    void onQuit() {
        this->cleanupOsuEffect();
        PlayLayer::onQuit();
    }

    void cleanupOsuEffect() {
        this->stopActionByTag(m_fields->m_actionTag);
        m_fields->m_isDead = false;
        m_fields->m_hasTriggeredDeath = false;
        m_fields->m_isFading = false;
        m_fields->m_time = 0.0f;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            // Reset background music pitch về 1.0
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                log::info("Background music pitch reset to 1.0");
            }
            
            // Reset global channel
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(1.0f);
            }
            
            // Reset tất cả các channel khác
            for (auto& channel : fmod->m_channels) {
                if (channel) {
                    channel->setPitch(1.0f);
                }
            }
        }
    }
};