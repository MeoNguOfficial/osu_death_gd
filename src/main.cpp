#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    static inline int priority = -99999;

    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
        int m_actionTag = 1001;
        bool m_hasStarted = false;
        bool m_isFading = false;  // Thêm flag để kiểm soát fade
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        m_fields->m_hasStarted = true;
        m_fields->m_isFading = false;
        
        this->cleanupOsuEffect();
        return true;
    }

    void applyOsuPitch() {
        // CHỈ chạy khi đã chết VÀ đang trong quá trình fade
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
                // Kết thúc fade
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
        // CHỈ trigger khi chưa chết và đã start level
        if (m_fields->m_hasStarted && !m_fields->m_isDead) {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
            m_fields->m_isFading = true;  // Bắt đầu fade

            // Dừng action cũ nếu có
            this->stopActionByTag(m_fields->m_actionTag);

            // Tạo action lặp để update pitch mỗi frame
            auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::applyOsuPitch));
            auto repeat = CCRepeatForever::create(call);
            repeat->setTag(m_fields->m_actionTag);
            
            this->runAction(repeat);
            log::info("Osu Death triggered - Starting pitch fade");
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() {
        this->cleanupOsuEffect();
        PlayLayer::resetLevel();
        // Reset flags khi reset level
        m_fields->m_isDead = false;
        m_fields->m_isFading = false;
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