#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class OsuDeathEffect {
private:
    static inline OsuDeathEffect* m_instance = nullptr;
    
    float m_time = 0.f;
    float m_duration = 1.f;
    float m_ogMusicPitch = 1.f;
    float m_ogSfxPitch = 1.f;
    bool m_isFading = false;
    bool m_hasTriggered = false;
    CCNode* m_target = nullptr;
    int m_actionTag = 1001;
    
    void update(float dt) {
        if (!m_isFading) return;
        
        m_time += dt;
        float progress = 1.f - std::min(m_time / m_duration, 1.f);
        
        if (progress <= 0.f) {
            m_isFading = false;
            cleanup();
            
            auto fmod = FMODAudioEngine::sharedEngine();
            if (fmod && fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPaused(true);
            }
            return;
        }
        
        float finalPitch = progress * progress;
        
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(m_ogMusicPitch * finalPitch);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }
            
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(m_ogSfxPitch * finalPitch);
            }
        }
    }
    
    void cleanup() {
        if (m_target) {
            m_target->stopActionByTag(m_actionTag);
            m_target->unschedule(schedule_selector(OsuDeathEffect::update));
        }
        
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(m_ogMusicPitch);
            }
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(m_ogSfxPitch);
            }
        }
        
        m_isFading = false;
        m_hasTriggered = false;
        m_target = nullptr;
    }
    
public:
    static OsuDeathEffect* get() {
        if (!m_instance) {
            m_instance = new OsuDeathEffect();
        }
        return m_instance;
    }
    
    void start(CCNode* target) {
        if (m_isFading || m_hasTriggered) return;
        
        m_hasTriggered = true;
        m_isFading = true;
        m_time = 0.f;
        m_target = target;
        
        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        m_duration = static_cast<float>(speedValue);
        if (m_duration <= 0.01f) m_duration = 1.f;
        
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->getPitch(&m_ogMusicPitch);
            }
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->getPitch(&m_ogSfxPitch);
            }
        }
        
        if (m_target) {
            m_target->schedule(schedule_selector(OsuDeathEffect::update), 1.f / 60.f);
        }
        
        log::info("Osu Death Effect started - Duration: {}", m_duration);
    }
    
    void stop() {
        cleanup();
    }
    
    bool isFading() { return m_isFading; }
};

class $modify(MyPlayLayer, PlayLayer)
{
    bool init(GJGameLevel *level, bool useReplay, bool dontRun)
    {
        if (!PlayLayer::init(level, useReplay, dontRun))
            return false;
        
        OsuDeathEffect::get()->stop();
        return true;
    }
    
    void destroyPlayer(PlayerObject *player, GameObject *obj)
    {
        PlayLayer::destroyPlayer(player, obj);
    }
    
    void resetLevel()
    {
        OsuDeathEffect::get()->stop();
        PlayLayer::resetLevel();
    }
    
    void onQuit()
    {
        OsuDeathEffect::get()->stop();
        PlayLayer::onQuit();
    }
};

class $modify(MyPlayerObject, PlayerObject)
{
    void playerDestroyed(bool p0)
    {
        PlayerObject::playerDestroyed(p0);
        
        auto playLayer = PlayLayer::get();
        if (playLayer) {
            OsuDeathEffect::get()->start(playLayer);
        }
    }
};

// Hook FMODAudioEngine
class $modify(MyFMODAudioEngine, FMODAudioEngine)
{
    void update(float dt) {
        FMODAudioEngine::update(dt);
        // Không cần làm gì thêm vì OsuDeathEffect tự update
    }
    
    // Override stopBackgroundMusic
    void stopBackgroundMusic() {
        if (OsuDeathEffect::get()->isFading()) {
            // Nếu đang fade, không cho stop music
            return;
        }
        // Gọi method gốc
        FMODAudioEngine::stopBackgroundMusic();
    }
    
    // Override pauseBackgroundMusic nếu cần
    void pauseBackgroundMusic() {
        if (OsuDeathEffect::get()->isFading()) {
            return;
        }
        FMODAudioEngine::pauseBackgroundMusic();
    }
};