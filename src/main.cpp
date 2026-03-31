#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// =============================================
// OSU DEATH EFFECT STATE
// =============================================
namespace OsuFail
{
    static bool  active   = false;
    static float time     = 0.0f;
    static float duration = 1.0f;

    // Lưu giá trị thực tế từ FMOD Channel
    static float startMusicPitch  = 1.0f;
    static float startSfxPitch    = 1.0f;
    static float startMusicVolume = 1.0f;
    static float startSfxVolume   = 1.0f;

    static constexpr float END_PITCH  = 0.15f; 
    static constexpr float END_VOLUME = 0.0f;

    float easeOut(float t) {
        return 1.0f - powf(1.0f - t, 3.0f);
    }

    void applyEffect()
    {
        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod || !fmod->m_backgroundMusicChannel || !fmod->m_globalChannel) return;

        float t = std::min(time / duration, 1.0f);
        float k = easeOut(t);

        // Nội suy giá trị
        float musicPitch = startMusicPitch - (startMusicPitch - END_PITCH) * k;
        float sfxPitch   = startSfxPitch   - (startSfxPitch   - END_PITCH) * k;
        float musicVol   = startMusicVolume - (startMusicVolume - END_VOLUME) * k;
        float sfxVol     = startSfxVolume   - (startSfxVolume   - END_VOLUME) * k;

        // Gửi lệnh trực tiếp tới FMOD Core thông qua các hàm trong FMOD.bro
        fmod->m_backgroundMusicChannel->setPitch(musicPitch);
        fmod->m_globalChannel->setPitch(sfxPitch);

        fmod->m_backgroundMusicChannel->setVolume(musicVol);
        fmod->m_globalChannel->setVolume(sfxVol);
    }

    void start(float dur)
    {
        if (active) return;

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod || !fmod->m_backgroundMusicChannel || !fmod->m_globalChannel) return;

        // Quan trọng: Lấy giá trị HIỆN TẠI từ channel
        fmod->m_backgroundMusicChannel->getPitch(&startMusicPitch);
        fmod->m_globalChannel->getPitch(&startSfxPitch);
        fmod->m_backgroundMusicChannel->getVolume(&startMusicVolume);
        fmod->m_globalChannel->getVolume(&startSfxVolume);

        active   = true;
        time     = 0.0f;
        duration = (dur > 0.01f) ? dur : 1.0f;
    }

    void reset()
    {
        if (!active && time == 0.0f) return;

        active = false;
        time   = 0.0f;

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        // Khôi phục Pitch về chuẩn
        if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPitch(1.0f);
        if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);

        // Khôi phục Volume về mức người dùng cài đặt trong game
        if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
        if (fmod->m_globalChannel) fmod->m_globalChannel->setVolume(fmod->m_sfxVolume);
    }
}

// =============================================
// PLAYLAYER HOOK
// =============================================
class $modify(MyPlayLayer, PlayLayer)
{
    void update(float dt)
    {
        PlayLayer::update(dt);

        if (OsuFail::active) {
            OsuFail::time += dt;
            OsuFail::applyEffect();

            if (OsuFail::time >= OsuFail::duration) {
                OsuFail::active = false;
                // Giữ nguyên trạng thái méo tiếng cho đến khi resetLevel
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object)
    {
        PlayLayer::destroyPlayer(player, object);

        // Chỉ chạy cho player chính và khi chưa hoàn thành level
        if (player != m_player1 && player != m_player2) return;
        if (m_hasCompletedLevel) return;

        float fadeSpeed = 1.0f;
        if (auto* mod = Mod::get()) {
            fadeSpeed = static_cast<float>(mod->getSettingValue<double>("fade-speed"));
        }

        OsuFail::start(fadeSpeed);
    }

    void resetLevel()
    {
        OsuFail::reset();
        PlayLayer::resetLevel();
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontRun)
    {
        OsuFail::reset();
        return PlayLayer::init(level, useReplay, dontRun);
    }

    void onQuit()
    {
        OsuFail::reset();
        PlayLayer::onQuit();
    }
};