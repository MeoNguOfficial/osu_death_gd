#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer)
{
    static inline int priority = -99999;

    struct Fields
    {
        bool m_isDead = false;
        float m_time = 0.0f;
        int m_actionTag = 1001;
        bool m_hasStarted = false;
        bool m_isFading = false;
        bool m_hasTriggeredDeath = false;
        float m_duration = 1.0f;  // Thêm duration vào đây
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
        m_fields->m_duration = 1.0f;

        this->cleanupOsuEffect();
        return true;
    }

    void onPlayerReallyDied()
    {
        if (m_fields->m_hasStarted && !m_fields->m_isDead && !m_fields->m_hasTriggeredDeath)
        {
            m_fields->m_isDead = true;
            m_fields->m_hasTriggeredDeath = true;
            m_fields->m_time = 0.0f;
            m_fields->m_isFading = true;
            
            // Lấy duration từ setting
            auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
            m_fields->m_duration = static_cast<float>(speedValue);
            if (m_fields->m_duration <= 0.01f)
                m_fields->m_duration = 1.0f;

            this->stopActionByTag(m_fields->m_actionTag);

            auto delay = CCDelayTime::create(1.0f / 60.0f);
            auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::applyOsuPitch));
            auto seq = CCSequence::create(delay, call, nullptr);
            auto repeat = CCRepeatForever::create(seq);
            repeat->setTag(m_fields->m_actionTag);

            this->runAction(repeat);
            log::info("Osu Death triggered - Player really died!");
        }
    }

    void applyOsuPitch()
    {
        if (!m_fields->m_isDead || !m_fields->m_isFading)
        {
            return;
        }

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod)
        {
            m_fields->m_time += 1.0f / 60.0f;
            float progress = 1.0f - (m_fields->m_time / m_fields->m_duration);

            if (progress <= 0.0f)
            {
                progress = 0.0f;
                m_fields->m_isFading = false;
                this->stopActionByTag(m_fields->m_actionTag);
                // Sau khi fade xong thì mới cho dừng nhạc hẳn
                if (fmod->m_backgroundMusicChannel)
                    fmod->m_backgroundMusicChannel->setPaused(true);
            }

            float finalPitch = progress * progress;

            // ÉP NHẠC NỀN (Music)
            if (fmod->m_backgroundMusicChannel)
            {
                fmod->m_backgroundMusicChannel->setPitch(finalPitch);
                // Quan trọng: Ngăn game pause nhạc quá sớm
                fmod->m_backgroundMusicChannel->setPaused(false);
            }

            // ÉP SFX/GLOBAL
            if (fmod->m_globalChannel)
            {
                fmod->m_globalChannel->setPitch(finalPitch);
            }
        }
    }

    void destroyPlayer(PlayerObject *player, GameObject *obj)
    {
        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel()
    {
        this->cleanupOsuEffect();
        PlayLayer::resetLevel();
        m_fields->m_isDead = false;
        m_fields->m_isFading = false;
        m_fields->m_hasTriggeredDeath = false;
        m_fields->m_time = 0.0f;
        m_fields->m_duration = 1.0f;
    }

    void onQuit()
    {
        this->cleanupOsuEffect();
        PlayLayer::onQuit();
    }

    void cleanupOsuEffect()
    {
        this->stopActionByTag(m_fields->m_actionTag);
        m_fields->m_isDead = false;
        m_fields->m_isFading = false;
        m_fields->m_hasTriggeredDeath = false;
        m_fields->m_time = 0.0f;
        m_fields->m_duration = 1.0f;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod)
        {
            if (fmod->m_backgroundMusicChannel)
            {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
            }
            if (fmod->m_globalChannel)
            {
                fmod->m_globalChannel->setPitch(1.0f);
            }
        }
    }
};

// Hook FMODAudioEngine để force set pitch mỗi frame
class $modify(MyFMODAudioEngine, FMODAudioEngine)
{
    void update(float dt)
    {
        // Gọi update gốc trước
        FMODAudioEngine::update(dt);
        
        // Force set pitch cho music nếu đang trong trạng thái fade
        auto playLayer = PlayLayer::get();
        if (playLayer)
        {
            auto myPlayLayer = reinterpret_cast<MyPlayLayer*>(playLayer);
            if (myPlayLayer && myPlayLayer->m_fields->m_isDead && myPlayLayer->m_fields->m_isFading)
            {
                // Tính progress và pitch
                float progress = 1.0f - (myPlayLayer->m_fields->m_time / myPlayLayer->m_fields->m_duration);
                if (progress < 0.0f) progress = 0.0f;
                float finalPitch = progress * progress;
                
                // Force set pitch cho music channel mỗi frame
                if (m_backgroundMusicChannel)
                {
                    m_backgroundMusicChannel->setPitch(finalPitch);
                    // Đảm bảo music không bị pause
                    m_backgroundMusicChannel->setPaused(false);
                }
                
                // Force set cho global channel
                if (m_globalChannel)
                {
                    m_globalChannel->setPitch(finalPitch);
                }
                
                log::debug("Force set pitch in update: {}", finalPitch);
            }
        }
    }
};

// PlayerObject hook
class $modify(MyPlayerObject, PlayerObject)
{
    void playerDestroyed(bool p0)
    {
        PlayerObject::playerDestroyed(p0);

        auto playLayer = PlayLayer::get();
        if (playLayer)
        {
            auto myPlayLayer = reinterpret_cast<MyPlayLayer *>(playLayer);
            myPlayLayer->onPlayerReallyDied();
        }
    }
};