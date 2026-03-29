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
        this->scheduleUpdate();
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (m_fields->m_isDead) {
            m_fields->m_time += dt;
            
            auto fmod = FMODAudioEngine::sharedEngine();
            if (!fmod) return;

            FMOD::ChannelGroup* masterGroup;
            fmod->m_system->getMasterChannelGroup(&masterGroup);

            if (masterGroup) {
                // duration chỉnh trong setting (ví dụ 0.5s hoặc 1.0s)
                float duration = Mod::get()->getSettingValue<double>("fade-speed");
                
                // Tính toán tiến trình từ 1.0 về 0.0 dựa trên duration
                // Progress = 1.0 - (Thời gian đã trôi qua / Tổng thời gian)
                float progress = 1.0f - (m_fields->m_time / duration);
                
                if (progress < 0.0f) progress = 0.0f;

                // Cập nhật Pitch
                masterGroup->setPitch(progress);
                
                // Nếu muốn mượt hơn nữa (dạng curve), Mèo có thể dùng:
                // masterGroup->setPitch(progress * progress);
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
        if (!m_fields->m_isDead) {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
        }
    }

    void resetLevel() {
        // Reset Pitch về 1.0 ngay lập tức trước khi reset màn
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
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