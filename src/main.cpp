#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    static inline int priority = -100;

    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
        int m_actionTag = 1001;
        bool m_hasStarted = false; 
        float m_initialCooldown = 0.0f; // Chốt chặn thời gian
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        m_fields->m_hasStarted = true;
        m_fields->m_initialCooldown = 0.0f;
        
        this->cleanupOsuEffect();
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        // Đếm thời gian từ lúc bắt đầu màn chơi
        m_fields->m_initialCooldown += dt;
    }

    void applyOsuPitch() {
        if (!m_fields->m_isDead || !m_fields->m_hasStarted) {
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

            float finalPitch = progress * progress;
            fmod->m_backgroundMusicChannel->setPitch(finalPitch);
            
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(finalPitch);
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Gọi hàm gốc trước để game cập nhật trạng thái player->m_isDead
        PlayLayer::destroyPlayer(player, obj);

        // ĐIỀU KIỆN FIX:
        // 1. Phải vào màn chơi > 0.5s (chống trigger lúc load)
        // 2. Nhân vật PHẢI thực sự chết (m_isDead của game là true)
        // 3. Chưa kích hoạt hiệu ứng trước đó
        if (m_fields->m_hasStarted && m_fields->m_initialCooldown > 0.5f && player->m_isDead && !m_fields->m_isDead) {
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

    void resetLevel() {
        this->cleanupOsuEffect();
        m_fields->m_initialCooldown = 0.0f; // Reset bộ đếm khi hồi sinh
        PlayLayer::resetLevel();
    }

    void onQuit() {
        this->cleanupOsuEffect();
        PlayLayer::onQuit();
    }

    void cleanupOsuEffect() {
        this->stopActionByTag(m_fields->m_actionTag);
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPitch(1.0f);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);
        }
    }
};