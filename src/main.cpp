#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

// Sử dụng priority cực cao để đè các mod khác
class $modify(MyPlayLayer, PlayLayer) {
    // Ép ưu tiên
    static void onModify(auto& self) {
        self.setHookPriority("PlayLayer::update", 99999);
        self.setHookPriority("PlayLayer::destroyPlayer", 99999);
    }

    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        log::info("Mod OsuDeath da vao man choi!"); // Check xem co dong nay trong console ko
        this->scheduleUpdate();
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (m_fields->m_isDead) {
            m_fields->m_time += dt;

            auto fmod = FMODAudioEngine::sharedEngine();
            if (fmod && fmod->m_system) {
                // Lay duration tu settings
                double durationSetting = Mod::get()->getSettingValue<double>("fade-speed");
                float duration = static_cast<float>(durationSetting);
                if (duration <= 0.01f) duration = 0.5f;

                float progress = 1.0f - (m_fields->m_time / duration);
                if (progress < 0.0f) progress = 0.0f;

                FMOD::ChannelGroup* masterGroup;
                fmod->m_system->getMasterChannelGroup(&masterGroup);

                if (masterGroup) {
                    float finalPitch = progress * progress;
                    masterGroup->setPitch(finalPitch);
                    
                    // Neu van ko nghe thay gi, thu ep ca Volume
                    // masterGroup->setVolume(progress);

                    // LOG BAT BUOC PHAI CO
                    log::info("Pitch dang giam: {}", finalPitch);
                }
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
        
        // Neu dam vao gai ma ko co log nay -> Hook bi hong
        log::info("Xac nhan hèo!"); 

        if (!m_fields->m_isDead) {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
        }
    }

    void resetLevel() {
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_system) {
            FMOD::ChannelGroup* masterGroup;
            fmod->m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) {
                masterGroup->setPitch(1.0f);
            }
        }
        PlayLayer::resetLevel();
    }
};