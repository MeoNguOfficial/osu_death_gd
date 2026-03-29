#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
        float m_ogMusicPitch = 1.0f;
        float m_ogSfxPitch = 1.0f;
    };

    void update(float dt) {
        PlayLayer::update(dt);

        if (m_fields->m_isDead) {
            m_fields->m_time += dt;

            auto fmod = FMODAudioEngine::sharedEngine();
            if (fmod && fmod->m_backgroundMusicChannel && fmod->m_globalChannel) {
                
                double durationSetting = Mod::get()->getSettingValue<double>("fade-speed");
                float duration = static_cast<float>(durationSetting);
                if (duration <= 0.01f) duration = 0.5f;

                float progress = 1.0f - (m_fields->m_time / duration);
                if (progress < 0.0f) progress = 0.0f;

                // Áp dụng Pitch giống hệt file FadeOut.hpp
                float finalPitch = progress * progress;
                fmod->m_backgroundMusicChannel->setPitch(m_fields->m_ogMusicPitch * finalPitch);
                fmod->m_globalChannel->setPitch(m_fields->m_ogSfxPitch * finalPitch);
                
                // Log để Mèo soi Console
                log::info("Osu Pitching: {}", finalPitch);
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (!m_fields->m_isDead) {
            auto fmod = FMODAudioEngine::sharedEngine();
            if (fmod) {
                // Lưu lại Pitch gốc trước khi bóp méo (giống file hpp)
                fmod->m_backgroundMusicChannel->getPitch(&m_fields->m_ogMusicPitch);
                fmod->m_globalChannel->getPitch(&m_fields->m_ogSfxPitch);
            }
            
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
            log::info("Trigger Osu Death Effect!");
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() {
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel && fmod->m_globalChannel) {
            // Trả lại Pitch gốc ngay khi reset
            fmod->m_backgroundMusicChannel->setPitch(m_fields->m_ogMusicPitch);
            fmod->m_globalChannel->setPitch(m_fields->m_ogSfxPitch);
        }
        
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        PlayLayer::resetLevel();
    }
};