#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        this->scheduleUpdate(); // Ép chạy update
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        // Kiểm tra biến dead trong Fields
        if (m_fields->m_isDead) {
            m_fields->m_time += dt;

            auto fmod = FMODAudioEngine::sharedEngine();
            if (fmod && fmod->m_system) {
                float duration = static_cast<float>(Mod::get()->getSettingValue<double>("fade-speed"));
                if (duration <= 0.01f) duration = 0.5f;

                float progress = 1.0f - (m_fields->m_time / duration);
                if (progress < 0.0f) progress = 0.0f;

                FMOD::ChannelGroup* masterGroup;
                fmod->m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup) {
                    float finalPitch = progress * progress;
                    masterGroup->setPitch(finalPitch);
                    // Dòng này PHẢI xuất hiện trong log nếu update đang chạy
                    log::info("Hieu ung dang chay: Pitch = {}", finalPitch);
                }
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Chỉ kích hoạt hiệu ứng nếu chưa chết (tránh spam log như Mèo thấy)
        if (!m_fields->m_isDead) {
            log::info("--- KICH HOAT HIEU UNG OSU DEATH ---"); 
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() {
        // Reset ngay lập tức khi hồi sinh
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_system) {
            FMOD::ChannelGroup* masterGroup;
            fmod->m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) {
                masterGroup->setPitch(1.0f);
            }
        }
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        PlayLayer::resetLevel();
    }
};