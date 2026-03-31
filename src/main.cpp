#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Các biến điều khiển trạng thái hiệu ứng
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;
static float s_originalFreq = 0.f;
static bool s_hasFreq = false;

// 1. Hook FMODAudioEngine
class $modify(MyFMODAudioEngine, FMODAudioEngine)
{
    void update(float dt)
    {
        // Gọi hàm gốc trước
        FMODAudioEngine::update(dt);

        if (!s_isDeadEffect)
            return;

        // =========================
        // 1. Ép pitch toàn bộ (SFX)
        // =========================
        if (m_system)
        {
            FMOD::ChannelGroup *masterGroup = nullptr;
            m_system->getMasterChannelGroup(&masterGroup);

            if (masterGroup)
            {
                masterGroup->setPitch(s_targetPitch);
            }
        }

        // =========================
        // 2. Ép pitch MUSIC (QUAN TRỌNG)
        // =========================
        if (m_backgroundMusicChannel)
        {
            // 🔥 Fix 1: Double-set để chống override
            m_backgroundMusicChannel->setPitch(s_targetPitch);
            m_backgroundMusicChannel->setPitch(s_targetPitch);
            
            // 🔥 Fix 2: Fallback dùng frequency nếu cần
            if (!s_hasFreq)
            {
                m_backgroundMusicChannel->getFrequency(&s_originalFreq);
                if (s_originalFreq > 0)
                    s_hasFreq = true;
            }
            
            if (s_hasFreq && s_originalFreq > 0)
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
        // 3. Backup: global channel (một số SFX đặc biệt)
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

// 2. Hook PlayLayer
class $modify(MyPlayLayer, PlayLayer)
{
    bool init(GJGameLevel *level, bool useReplay, bool dontRun)
    {
        if (!PlayLayer::init(level, useReplay, dontRun))
            return false;

        // Reset effect khi vào level mới
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

        // Lấy duration từ setting
        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        s_duration = (speedValue <= 0.05f) ? 1.0f : static_cast<float>(speedValue);

        // Bắt đầu schedule
        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));

        log::info("Osu Death Effect started - Duration: {} seconds", s_duration);
    }

    void updateOsuFade(float dt)
    {
        if (!s_isDeadEffect)
        {
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));
            return;
        }

        // 🔥 Exponential decay fade (giống osu hơn)
        // s_targetPitch = s_targetPitch - (dt / s_duration) * s_targetPitch;
        // Hoặc linear fade (đơn giản hơn)
        float decrement = dt / s_duration;
        s_targetPitch -= decrement;

        if (s_targetPitch <= 0.01f)
        {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;
            s_hasFreq = false;
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

            // Dừng nhạc sau khi fade xong
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
        // Reset effect
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        s_hasFreq = false;
        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        // Reset audio
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod)
        {
            if (fmod->m_system)
            {
                FMOD::ChannelGroup *masterGroup = nullptr;
                fmod->m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup)
                {
                    masterGroup->setPitch(1.0f);
                }
            }

            if (fmod->m_backgroundMusicChannel)
            {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
                fmod->m_backgroundMusicChannel->setPaused(false);
                
                // Reset frequency nếu đã dùng
                if (s_originalFreq > 0)
                {
                    fmod->m_backgroundMusicChannel->setFrequency(s_originalFreq);
                }
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
        // Reset effect
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        s_hasFreq = false;
        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        // Reset audio
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod)
        {
            if (fmod->m_system)
            {
                FMOD::ChannelGroup *masterGroup = nullptr;
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
                
                if (s_originalFreq > 0)
                {
                    fmod->m_backgroundMusicChannel->setFrequency(s_originalFreq);
                }
            }

            if (fmod->m_globalChannel)
            {
                fmod->m_globalChannel->setPitch(1.0f);
            }
        }

        PlayLayer::onQuit();
    }
};

// 3. Hook PlayerObject
class $modify(MyPlayerObject, PlayerObject)
{
    void playDeathEffect()
    {
        PlayerObject::playDeathEffect();

        auto playLayer = PlayLayer::get();
        if (playLayer)
        {
            static_cast<MyPlayLayer *>(playLayer)->onPlayerReallyDied();
        }
    }
};