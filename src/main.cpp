#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
    };

    // Hàm thực hiện ép Pitch trực tiếp
    void applyOsuPitch() {
        if (!m_fields->m_isDead) return;

        m_fields->m_time += 1.0f / 60.0f; // Giả lập dt 60fps
        
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            float duration = static_cast<float>(Mod::get()->getSettingValue<double>("fade-speed"));
            if (duration <= 0.01f) duration = 0.5f;

            float progress = 1.0f - (m_fields->m_time / duration);
            if (progress < 0.0f) progress = 0.0f;

            float finalPitch = progress * progress;
            fmod->m_backgroundMusicChannel->setPitch(finalPitch);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(finalPitch);
            
            // Ép log xuất hiện để Mèo thấy
            log::info("Pitch dang ep: {}", finalPitch);
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (!m_fields->m_isDead) {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
            log::info("Trigger Osu Death Effect!");

            // Tạo một Action chạy liên tục để ép Pitch mà không cần hàm update()
            auto delay = CCDelayTime::create(1.0f / 60.0f);
            auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::applyOsuPitch));
            auto seq = CCSequence::create(delay, call, nullptr);
            this->runAction(CCRepeatForever::create(seq));
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() {
        this->stopAllActions(); // Dừng việc ép Pitch
        
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPitch(1.0f);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);
        }

        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        PlayLayer::resetLevel();
    }
};