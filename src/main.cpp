#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

// Ép Geode ưu tiên chạy Hook của mình để không bị Mega Hack đè mất logic reset
class $modify(MyPlayLayer, PlayLayer) {
    // Thiết lập ưu tiên cực cao
    static void onModify(SettingNode* node) {
        (void)node;
        auto mut = Mod::get()->patch(nullptr, 0); // Placeholder để gọi onModify
    }

    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
        int m_actionTag = 1001;
        bool m_hasStarted = false; // Biến phụ để check xem màn chơi thực sự bắt đầu chưa
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        // Reset tuyệt đối khi vào màn
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        m_fields->m_hasStarted = true; // Chỉ đánh dấu bắt đầu sau khi init xong
        
        this->cleanupOsuEffect();
        return true;
    }

    void applyOsuPitch() {
        // Nếu chưa chết HOẶC chưa thực sự bắt đầu màn chơi thì dừng ngay
        if (!m_fields->m_isDead || !m_fields->m_hasStarted) {
            this->stopActionByTag(m_fields->m_actionTag);
            return;
        }

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            float duration = static_cast<float>(Mod::get()->getSettingValue<double>("fade-speed"));
            if (duration <= 0.01f) duration = 0.5f;

            m_fields->m_time += 1.0f / 60.0f;
            float progress = 1.0f - (m_fields->m_time / duration);

            if (progress <= 0.0f) {
                progress = 0.0f;
                this->stopActionByTag(m_fields->m_actionTag);
            }

            float finalPitch = progress * progress;
            fmod->m_backgroundMusicChannel->setPitch(finalPitch);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(finalPitch);
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Chỉ kích hoạt nếu màn chơi đã load xong và chưa chết
        if (m_fields->m_hasStarted && !m_fields->m_isDead) {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;

            this->stopActionByTag(m_fields->m_actionTag);

            auto delay = CCDelayTime::create(1.0f / 60.0f);
            auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::applyOsuPitch));
            auto seq = CCSequence::create(delay, call, nullptr);
            auto repeat = CCRepeatForever::create(seq);
            repeat->setTag(m_fields->m_actionTag);
            
            this->runAction(repeat);
            log::info("Osu Death triggered hop le!");
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() {
        this->cleanupOsuEffect();
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