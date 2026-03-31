#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// =========================
// GLOBAL STATE
// =========================
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;

// 🔥 Frequency fallback
static float s_originalFreq = 0.f;
static bool s_hasFreq = false;

// =========================
// FMOD HOOK
// =========================
class $modify(MyFMODAudioEngine, FMODAudioEngine)
{
    void update(float dt)
    {
        FMODAudioEngine::update(dt);

        if (!s_isDeadEffect)
            return;

        // =========================
        // 1. SFX (master)
        // =========================
        if (m_system)
        {
            FMOD::ChannelGroup* masterGroup = nullptr;
            m_system->getMasterChannelGroup(&masterGroup);

            if (masterGroup)
            {
                masterGroup->setPitch(s_targetPitch);
            }
        }

        // =========================
        // 2. MUSIC (CRITICAL)
        // =========================
        if (m_backgroundMusicChannel)
        {
            // Lưu frequency gốc (1 lần)
            if (!s_hasFreq)
            {
                m_backgroundMusicChannel->getFrequency(&s_originalFreq);
                s_hasFreq = true;
            }

            // 🔥 Double set để chống override
            m_backgroundMusicChannel->setPitch(s_targetPitch);
            m_backgroundMusicChannel->setPitch(s_targetPitch);

            // 🔥 Fallback mạnh (custom song / stream)
            if (s_originalFreq > 0.0f)
            {
                m_backgroundMusicChannel->setFrequency(s_originalFreq * s_targetPitch);
            }

            // Đảm bảo không bị pause
            bool isPaused = false;
            m_backgroundMusicChannel->getPaused(&isPaused);

            if (isPaused && s_targetPitch > 0.05f)
            {
                m_backgroundMusicChannel->setPaused(false);
            }
        }

        // =========================
        // 3. GLOBAL CHANNEL (backup SFX)
        // =========================
        if (m_globalChannel)
        {
            m_globalChannel->setPitch(s_targetPitch);
        }
    }

    void stopAllMusic(bool p0)
    {
        if (s_isDeadEffect)
            return;
        FMODAudioEngine::stopAllMusic(p0);
    }
};

// =========================
// PLAYLAYER HOOK
// =========================
class $modify(MyPlayLayer, PlayLayer)
{
    bool init(GJGameLevel* level, bool useReplay, bool dontRun)
    {
        if (!PlayLayer::init(level, useReplay, dontRun))
            return false;

        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        s_hasFreq = false;

        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));
        return true;
    }

    void onPlayerReallyDied()
    {
        if (s_isDeadEffect)
            return;

        s_isDeadEffect = true;
        s_targetPitch = 1.0f;
        s_hasFreq = false;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        s_duration = (speedValue <= 0.05f) ? 1.0f : static_cast<float>(speedValue);

        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));

        log::info("Osu Death Effect started - Duration: {}", s_duration);
    }

    void updateOsuFade(float dt)
    {
        if (!s_isDeadEffect)
        {
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));
            return;
        }

        // 🔥 Exponential fade (mượt hơn linear)
        float factor = dt / s_duration;
        s_targetPitch -= s_targetPitch * factor;

        if (s_targetPitch <= 0.01f)
        {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;

            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

            auto fmod = FMODAudioEngine::sharedEngine();
            if (fmod && fmod->m_backgroundMusicChannel)
            {
                fmod->m_backgroundMusicChannel->setPaused(true);
            }

            log::info("Osu Death Effect finished");
        }
    }

    void resetLevel()
    {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        s_hasFreq = false;

        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod)
        {
            if (fmod->m_system)
            {
                FMOD::ChannelGroup* masterGroup = nullptr;
                fmod->m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup)
                {
                    masterGroup->setPitch(1.0f);
                }
            }

            if (fmod->m_backgroundMusicChannel)
            {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }

            if (fmod->m_globalChannel)
            {
                fmod->m_globalChannel->setPitch(1.0f);
            }
        }

        PlayLayer::resetLevel();
    }

    void onQuit()
    {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        s_hasFreq = false;

        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod)
        {
            if (fmod->m_system)
            {
                FMOD::ChannelGroup* masterGroup = nullptr;
                fmod->m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup)
                {
                    masterGroup->setPitch(1.0f);
                }
            }

            if (fmod->m_backgroundMusicChannel)
            {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }

            if (fmod->m_globalChannel)
            {
                fmod->m_globalChannel->setPitch(1.0f);
            }
        }

        PlayLayer::onQuit();
    }
};

// =========================
// PLAYER HOOK
// =========================
class $modify(MyPlayerObject, PlayerObject)
{
    void playDeathEffect()
    {
        PlayerObject::playDeathEffect();

        if (auto pl = PlayLayer::get())
        {
            static_cast<MyPlayLayer*>(pl)->onPlayerReallyDied();
        }
    }
};
