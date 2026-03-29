#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        bool m_isDead = false;
        float m_currentPitch = 1.0f;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        this->scheduleUpdate();
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine || !engine->m_system) return;

        FMOD::ChannelGroup* masterGroup;
        engine->m_system->getMasterChannelGroup(&masterGroup);
        if (!masterGroup) return;

        if (m_fields->m_isDead) {
            if (m_fields->m_currentPitch > 0.0f) {
                // Giảm dần pitch. 2.5f là tốc độ (có thể chỉnh lại)
                m_fields->m_currentPitch -= dt * 2.5f; 
                if (m_fields->m_currentPitch < 0.0f) m_fields->m_currentPitch = 0.0f;
                masterGroup->setPitch(m_fields->m_currentPitch);
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Nếu đã chết rồi thì không trigger lại nữa (chặn lỗi lặp âm)
        if (m_fields->m_isDead) return;

        PlayLayer::destroyPlayer(player, obj);
        m_fields->m_isDead = true;
    }

    void resetLevel() {
        // Khi bấm Reset, ngay lập tức ép Pitch về 1.0 để tránh lag âm thanh
        auto engine = FMODAudioEngine::sharedEngine();
        if (engine) {
            FMOD::ChannelGroup* masterGroup;
            engine->m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) {
                masterGroup->setPitch(1.0f);
            }
        }

        m_fields->m_isDead = false;
        m_fields->m_currentPitch = 1.0f;
        
        PlayLayer::resetLevel();
    }
};