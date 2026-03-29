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

        if (m_fields->m_isDead && m_fields->m_currentPitch > 0.0f) {
            auto engine = FMODAudioEngine::sharedEngine();
            if (engine && engine->m_system) {
                FMOD::ChannelGroup* masterGroup;
                engine->m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup) {
                    // Lấy giá trị từ Setting mà Mèo chỉnh trong game
                    float fadeSpeed = Mod::get()->getSettingValue<double>("fade-speed");
                    
                    m_fields->m_currentPitch -= dt * fadeSpeed;
                    if (m_fields->m_currentPitch < 0.0f) m_fields->m_currentPitch = 0.0f;
                    
                    masterGroup->setPitch(m_fields->m_currentPitch);
                }
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
        // Kích hoạt hiệu ứng ngay sau khi nổ xác
        if (!m_fields->m_isDead) {
            m_fields->m_isDead = true;
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        m_fields->m_isDead = false;
        m_fields->m_currentPitch = 1.0f;

        auto engine = FMODAudioEngine::sharedEngine();
        if (engine) {
            FMOD::ChannelGroup* masterGroup;
            engine->m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) {
                masterGroup->setPitch(1.0f);
            }
        }
    }
};