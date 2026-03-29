#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    // Ép ưu tiên cực cao để chạy TRƯỚC các mod khác
    static inline int priority = 999999; 

    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
        int m_actionTag = 1001;
        float m_initialCooldown = 0.0f;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        m_fields->m_initialCooldown = 0.0f;
        
        this->cleanupOsuEffect();
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        m_fields->m_initialCooldown += dt;
    }

    void applyOsuPitch() {
        // Chỉ chạy khi thực sự đang ở trong PlayLayer
        if (!m_fields->m_isDead || !GameManager::sharedEngine()->getPlayLayer()) {
            this->stopActionByTag(m_fields->m_actionTag);
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
                this->stopActionByTag(m_fields->m_actionTag);
            }

            float finalPitch = std::clamp(progress * progress, 0.0f, 1.0f);
            fmod->m_backgroundMusicChannel->setPitch(finalPitch);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(finalPitch);
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // KIỂM TRA TỐI THƯỢNG:
        // 1. Màn chơi phải chạy được ít nhất 0.5 giây (loại bỏ trigger lúc load)
        // 2. Phải thực sự đang trong trạng thái chơi (không phải menu/randomizer gọi)
        if (m_fields->m_initialCooldown > 0.5f && !m_fields->m_isDead) {
            
            // Một số mod gọi destroyPlayer nhưng không làm chết player thật
            // Ta kiểm tra xem player thực sự có đang nổ tung không
            if (player->m_isDead) { 
                m_fields->m_isDead = true;
                m_fields->m_time = 0.0f;

                this->stopActionByTag(m_fields->m_actionTag);

                auto delay = CCDelayTime::create(1.0f / 60.0f);
                auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::applyOsuPitch));
                auto seq = CCSequence::create(delay, call, nullptr);
                auto repeat = CCRepeatForever::create(seq);
                repeat->setTag(m_fields->m_actionTag);
                
                this->runAction(repeat);
                log::info("Osu Death triggered hop le tai {:.2f}s", m_fields->m_initialCooldown);
            }
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() {
        this->cleanupOsuEffect();
        m_fields->m_initialCooldown = 0.0f;
        PlayLayer::resetLevel();
    }

    void cleanupOsuEffect() {
        this->stopActionByTag(m_fields->m_actionTag);
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPitch(1.0f);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);
        }
    }
};