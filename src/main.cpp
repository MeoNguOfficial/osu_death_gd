#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        self.setHookPriority("PlayLayer::update", 99999);
        self.setHookPriority("PlayLayer::destroyPlayer", 99999);
        self.setHookPriority("PlayLayer::resetLevel", 99999);
    }

    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;

        FMOD::ChannelGroup* m_musicGroup = nullptr;
        bool m_logged = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;

        log::info("OsuDeath loaded (2.208 safe)");
        this->scheduleUpdate();
        return true;
    }

    // 🔍 Scan group động (quan trọng cho 2.208)
    FMOD::ChannelGroup* findMusicGroup(FMOD::System* system) {
        if (!system) return nullptr;

        FMOD::ChannelGroup* master = nullptr;
        system->getMasterChannelGroup(&master);
        if (!master) return nullptr;

        int count = 0;
        master->getNumGroups(&count);

        for (int i = 0; i < count; i++) {
            FMOD::ChannelGroup* grp = nullptr;
            master->getGroup(i, &grp);

            if (!grp) continue;

            char name[256];
            grp->getName(name, 256);

            // 🎯 Heuristic: tìm group chứa "music"
            std::string n = name;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);

            if (n.find("music") != std::string::npos) {
                log::info("Found music group: {}", name);
                return grp;
            }
        }

        // fallback: lấy group đầu (thường là music trong GD)
        FMOD::ChannelGroup* fallback = nullptr;
        if (count > 0) {
            master->getGroup(0, &fallback);
            log::info("Fallback group[0]");
        }

        return fallback;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (!m_fields->m_isDead) return;

        m_fields->m_time += dt;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (!fmod || !fmod->m_system) return;

        // 🔁 Reacquire nếu mất (GD 2.208 hay recreate)
        if (!m_fields->m_musicGroup) {
            m_fields->m_musicGroup = findMusicGroup(fmod->m_system);
        }

        auto musicGroup = m_fields->m_musicGroup;
        if (!musicGroup) return;

        float duration = std::max(0.01f,
            (float)Mod::get()->getSettingValue<double>("fade-speed")
        );

        float progress = 1.0f - (m_fields->m_time / duration);
        progress = std::clamp(progress, 0.0f, 1.0f);

        // 🎯 osu-like easing
        float finalPitch = progress * progress;

        musicGroup->setPitch(finalPitch);

        // ❗ KHÔNG đụng volume → tránh bug âm thanh to

        if (!m_fields->m_logged) {
            log::info("Applying osu death effect...");
            m_fields->m_logged = true;
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);

        log::info("Player died -> trigger effect");

        if (!m_fields->m_isDead) {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
            m_fields->m_logged = false;
        }
    }

    void resetLevel() {
        auto fmod = FMODAudioEngine::sharedEngine();

        if (fmod && fmod->m_system && m_fields->m_musicGroup) {
            m_fields->m_musicGroup->setPitch(1.0f);
        }

        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        m_fields->m_musicGroup = nullptr;
        m_fields->m_logged = false;

        PlayLayer::resetLevel();
    }
};
