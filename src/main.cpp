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

        // Chỉ xử lý giảm pitch nếu đang trong trạng thái hẻo
        if (m_fields->m_isDead) {
            auto engine = FMODAudioEngine::sharedEngine();
            if (engine && engine->m_system) {
                FMOD::ChannelGroup* masterGroup;
                engine->m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup) {
                    if (m_fields->m_currentPitch > 0.0f) {
                        m_fields->m_currentPitch -= dt * 2.5f;
                        if (m_fields->m_currentPitch < 0.0f) m_fields->m_currentPitch = 0.0f;
                        masterGroup->setPitch(m_fields->m_currentPitch);
                    }
                }
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // LUÔN gọi hàm gốc trước để game xử lý va chạm và nổ xác
        PlayLayer::destroyPlayer(player, obj);

        // Sau đó mới bật trạng thái méo tiếng (nếu chưa bật)
        if (!m_fields->m_isDead) {
            m_fields->m_isDead = true;
        }
    }

    void resetLevel() {
        // Gọi hàm gốc để reset màn chơi
        PlayLayer::resetLevel();

        // Trả lại âm thanh ngay lập tức
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