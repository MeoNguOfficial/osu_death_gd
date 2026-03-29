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

        if (!m_fields->m_isDead) return;

        m_fields->m_time += dt;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (!fmod || !fmod->m_backgroundMusicChannel) return;

        auto music = fmod->m_backgroundMusicChannel;

        // ⚠️ tránh chia 0
        float duration = std::max(0.01f,
            (float)Mod::get()->getSettingValue<double>("fade-speed")
        );

        // ⏱ progress từ 1 -> 0
        float progress = 1.0f - (m_fields->m_time / duration);
        progress = std::clamp(progress, 0.0f, 1.0f);

        // 🎯 easing giống osu (quan trọng)
        float eased = progress * progress;              // basic ease-out
        // float eased = pow(progress, 2.5f);           // mạnh hơn nếu muốn

        // 🎵 APPLY EFFECT
        music->setPitch(eased);

        // 🔉 giảm volume cùng lúc (rất giống osu)
        music->setVolume(eased);

        // 🛑 tránh pitch = 0 (FMOD dễ glitch)
        if (eased <= 0.01f) {
            music->setPitch(0.01f);
            music->setVolume(0.0f);
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
        auto fmod = FMODAudioEngine::sharedEngine();

        if (fmod && fmod->m_backgroundMusicChannel) {
            auto music = fmod->m_backgroundMusicChannel;

            // 🔄 reset audio về bình thường
            music->setPitch(1.0f);
            music->setVolume(1.0f);
        }

        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;

        PlayLayer::resetLevel();
    }
};
