#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class $modify(MyPlayerObject, PlayerObject) {
    void playerDestroyed(bool p0) {
        PlayerObject::playerDestroyed(p0);
        
        // Khi player thực sự bị destroy, trigger từ PlayLayer
        auto playLayer = PlayLayer::get();
        if (playLayer) {
            // Gọi hàm để trigger death effect
            static_cast<MyPlayLayer*>(playLayer)->onPlayerReallyDied();
        }
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    static inline int priority = -99999;

    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
        int m_actionTag = 1001;
        bool m_hasStarted = false;
        bool m_isFading = false;
        bool m_hasTriggeredDeath = false;  // Đánh dấu đã trigger death effect
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        m_fields->m_hasStarted = true;
        m_fields->m_isFading = false;
        m_fields->m_hasTriggeredDeath = false;
        
        this->cleanupOsuEffect();
        return true;
    }

    void onPlayerReallyDied() {
        // Chỉ trigger khi thực sự chết và chưa trigger
        if (m_fields->m_hasStarted && !m_fields->m_isDead && !m_fields->m_hasTriggeredDeath) {
            m_fields->m_isDead = true;
            m_fields->m_hasTriggeredDeath = true;
            m_fields->m_time = 0.0f;
            m_fields->m_isFading = true;

            this->stopActionByTag(m_fields->m_actionTag);

            auto delay = CCDelayTime::create(1.0f / 60.0f);
            auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::applyOsuPitch));
            auto seq = CCSequence::create(delay, call, nullptr);
            auto repeat = CCRepeatForever::create(seq);
            repeat->setTag(m_fields->m_actionTag);
            
            this->runAction(repeat);
            log::info("Osu Death triggered - Player really died!");
        }
    }

    void applyOsuPitch() {
        if (!m_fields->m_isDead || !m_fields->m_isFading) {
            return;
        }

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
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
            fmod->m_backgroundMusicChannel->setPitch(finalPitch);
            
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(finalPitch);
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
        // Không làm gì ở đây nữa, để PlayerObject::playerDestroyed trigger
    }

    void resetLevel() {
        this->cleanupOsuEffect();
        PlayLayer::resetLevel();
        m_fields->m_isDead = false;
        m_fields->m_isFading = false;
        m_fields->m_hasTriggeredDeath = false;
        m_fields->m_time = 0.0f;
    }

    void onQuit() {
        this->cleanupOsuEffect();
        PlayLayer::onQuit();
    }

    void cleanupOsuEffect() {
        this->stopActionByTag(m_fields->m_actionTag);
        m_fields->m_isDead = false;
        m_fields->m_isFading = false;
        m_fields->m_hasTriggeredDeath = false;
        m_fields->m_time = 0.0f;

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