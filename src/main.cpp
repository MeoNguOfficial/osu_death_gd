#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
        int m_actionTag = 1001; // Tag để định danh hành động ép Pitch
    };

    void applyOsuPitch() {
        if (!m_fields->m_isDead) return;

        float duration = static_cast<float>(Mod::get()->getSettingValue<double>("fade-speed"));
        if (duration <= 0.01f) duration = 0.5f;

        m_fields->m_time += 1.0f / 60.0f;
        float progress = 1.0f - (m_fields->m_time / duration);

        if (progress <= 0.0f) {
            progress = 0.0f;
            this->stopActionByTag(m_fields->m_actionTag); // Dừng hẳn khi đã hết effect
        }

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            float finalPitch = progress * progress;
            fmod->m_backgroundMusicChannel->setPitch(finalPitch);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(finalPitch);
        }
    }

    // Hook resetLevel để xử lý Fast Restart (Phím R)
    void resetLevel() {
        this->cleanupOsuEffect();
        PlayLayer::resetLevel();
    }

    // Hook onQuit để xử lý khi thoát ra menu
    void onQuit() {
        this->cleanupOsuEffect();
        PlayLayer::onQuit();
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (!m_fields->m_isDead) {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;

            // Xóa hành động cũ nếu có trước khi chạy cái mới
            this->stopActionByTag(m_fields->m_actionTag);

            auto delay = CCDelayTime::create(1.0f / 60.0f);
            auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::applyOsuPitch));
            auto seq = CCSequence::create(delay, call, nullptr);
            auto repeat = CCRepeatForever::create(seq);
            repeat->setTag(m_fields->m_actionTag); // Gán tag để quản lý
            
            this->runAction(repeat);
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    // Hàm dùng chung để dọn dẹp effect
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