#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

namespace OsuFail
{
    static bool active   = false;
    static float time    = 0.0f;
    static float duration = 1.0f;

    static float startMusicPitch = 1.0f;
    static float startSfxPitch   = 1.0f;
    static const float endPitch  = 0.3f;

    float easeOut(float t)
    {
        return 1.0f - powf(1.0f - t, 3.0f);
    }

    void applyPitch()
    {
        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        float t = std::min(time / duration, 1.0f);
        float k = easeOut(t);

        float musicPitch = startMusicPitch + (endPitch - startMusicPitch) * k;
        float sfxPitch   = startSfxPitch   + (endPitch - startSfxPitch)   * k;

        fmod->m_backgroundMusicChannel->setPitch(musicPitch);
        fmod->m_globalChannel->setPitch(sfxPitch);
    }

    void start(float dur)
    {
        if (active) return; // đang chạy rồi thì không restart

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        // Lưu pitch gốc hiện tại
        fmod->m_backgroundMusicChannel->getPitch(&startMusicPitch);
        fmod->m_globalChannel->getPitch(&startSfxPitch);

        active   = true;
        time     = 0.0f;
        duration = (dur > 0.05f) ? dur : 1.0f;
    }

    void reset()
    {
        active = false;
        time   = 0.0f;

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        fmod->m_backgroundMusicChannel->setPitch(1.0f);
        fmod->m_globalChannel->setPitch(1.0f);
    }
}

// =============================================
// TICK effect trong update của PlayLayer
// =============================================
class $modify(MyPlayLayer, PlayLayer)
{
    void update(float dt)
    {
        PlayLayer::update(dt);

        if (!OsuFail::active) return;

        OsuFail::time += dt;
        OsuFail::applyPitch();

        if (OsuFail::time >= OsuFail::duration)
            OsuFail::active = false;
        // pitch giữ thấp cho đến khi resetLevel/onQuit
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontRun)
    {
        OsuFail::reset();
        return PlayLayer::init(level, useReplay, dontRun);
    }

    void resetLevel()
    {
        OsuFail::reset();
        PlayLayer::resetLevel();
    }

    void onQuit()
    {
        OsuFail::reset();
        PlayLayer::onQuit();
    }
};

// =============================================
// TRIGGER đúng chỗ: playerDied trên GJBaseGameLayer
// =============================================
class $modify(MyGJBaseGameLayer, GJBaseGameLayer)
{
    void playerDied(PlayerObject* player, bool p1, bool p2)
    {
        GJBaseGameLayer::playerDied(player, p1, p2);

        // Chỉ trigger cho player thật, không phải dual-mode dummy
        if (!PlayLayer::get()) return;
        if (!player->isVanillaPlayer()) return;

        // Chỉ trigger 1 lần dù player1 hay player2 chết
        if (OsuFail::active) return;

        float dur = 1.0f;
        if (auto* mod = Mod::get())
            dur = static_cast<float>(mod->getSettingValue<double>("fade-speed"));

        OsuFail::start(dur);
    }
};