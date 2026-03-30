#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer)
{
    // Đặt priority cực thấp để đảm bảo code mình chạy sau cùng trong 1 frame
    static inline int priority = -99999;

    struct Fields
    {
        bool m_isDead = false;
        float m_time = 0.0f;
        bool m_hasStarted = false;
        bool m_isFading = false;
        bool m_hasTriggeredDeath = false;
    };

    bool init(GJGameLevel *level, bool useReplay, bool dontRun)
    {
        if (!PlayLayer::init(level, useReplay, dontRun))
            return false;

        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        m_fields->m_hasStarted = true;
        m_fields->m_isFading = false;
        m_fields->m_hasTriggeredDeath = false;

        this->cleanupOsuEffect();
        return true;
    }

    // Chuyển logic sang update để ép pitch liên tục
    void update(float dt) {
        PlayLayer::update(dt);

        if (m_fields->m_isFading && m_fields->m_isDead) {
            this->applyOsuPitch(dt);
        }
    }

    void onPlayerReallyDied()
    {
        if (m_fields->m_hasStarted && !m_fields->m_isDead && !m_fields->m_hasTriggeredDeath)
        {
            m_fields->m_isDead = true;
            m_fields->m_hasTriggeredDeath = true;
            m_fields->m_time = 0.0f;
            m_fields->m_isFading = true;

            log::info("Osu Death triggered - Starting Full Update Override!");
        }
    }

    void applyOsuPitch(float dt)
    {
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod)
        {
            auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
            float duration = static_cast<float>(speedValue);
            if (duration <= 0.01f)
                duration = 1.0f;

            m_fields->m_time += dt; // Dùng dt thực tế thay vì 1/60 cố định
            float progress = 1.0f - (m_fields->m_time / duration);

            if (progress <= 0.0f)
            {
                progress = 0.0f;
                m_fields->m_isFading = false;
                
                if (fmod->m_backgroundMusicChannel)
                    fmod->m_backgroundMusicChannel->setPaused(true);
            }

            float finalPitch = progress * progress;

            // ÉP MUSIC: Chặn đứng việc game tự reset pitch hoặc pause nhạc
            if (fmod->m_backgroundMusicChannel)
            {
                fmod->m_backgroundMusicChannel->setPitch(finalPitch);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }

            // ÉP SFX/GLOBAL
            if (fmod->m_globalChannel)
            {
                fmod->m_globalChannel->setPitch(finalPitch);
            }
        }
    }

    void resetLevel()
    {
        this->cleanupOsuEffect();
        PlayLayer::resetLevel();
    }

    void onQuit()
    {
        this->cleanupOsuEffect();
        PlayLayer::onQuit();
    }

    void cleanupOsuEffect()
    {
        m_fields->m_isDead = false;
        m_fields->m_isFading = false;
        m_fields->m_hasTriggeredDeath = false;
        m_fields->m_time = 0.0f;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod)
        {
            if (fmod->m_backgroundMusicChannel)
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
            
            if (fmod->m_globalChannel)
                fmod->m_globalChannel->setPitch(1.0f);
        }
    }
};

class $modify(MyPlayerObject, PlayerObject)
{
    void playerDestroyed(bool p0)
    {
        PlayerObject::playerDestroyed(p0);

        auto playLayer = PlayLayer::get();
        if (playLayer)
        {
            auto myPlayLayer = static_cast<MyPlayLayer *>(playLayer);
            myPlayLayer->onPlayerReallyDied();
        }
    }
};