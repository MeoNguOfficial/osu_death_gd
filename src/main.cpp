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
        if (!fmod) return;

        float t = std::min(time / duration, 1.0f);
        float k = easeOut(t);

        // Tính toán giá trị nội suy
        float musicPitch = startMusicPitch - (startMusicPitch - END_PITCH) * k;
        float sfxPitch   = startSfxPitch   - (startSfxPitch   - END_PITCH) * k;
        float musicVol   = startMusicVolume - (startMusicVolume - END_VOLUME) * k;
        float sfxVol     = startSfxVolume   - (startSfxVolume   - END_VOLUME) * k;

        // Xử lý MUSIC
        if (fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPitch(musicPitch);
            fmod->m_backgroundMusicChannel->setVolume(musicVol);
        }

        // Xử lý SFX (Global Channel trong GD quản lý phần lớn SFX)
        if (fmod->m_globalChannel) {
            fmod->m_globalChannel->setPitch(sfxPitch);
            fmod->m_globalChannel->setVolume(sfxVol);
        }
    }

    void start(float dur)
    {
        if (active) return;

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        // Lấy giá trị hiện tại ngay khi chết để làm mốc bắt đầu
        if (fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->getPitch(&startMusicPitch);
            fmod->m_backgroundMusicChannel->getVolume(&startMusicVolume);
        }
        
        if (fmod->m_globalChannel) {
            fmod->m_globalChannel->getPitch(&startSfxPitch);
            fmod->m_globalChannel->getVolume(&startSfxVolume);
        }

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

        // Khôi phục về mặc định của Game
        if (fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPitch(1.0f);
            fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
        }

        if (fmod->m_globalChannel) {
            fmod->m_globalChannel->setPitch(1.0f);
            fmod->m_globalChannel->setVolume(fmod->m_sfxVolume);
        }
    }
}

// =============================================
// PLAYLAYER HOOK
// =============================================
class $modify(MyPlayLayer, PlayLayer)
{
    // Sử dụng post-update để đảm bảo giá trị của chúng ta được set SAU khi game update
    void update(float dt)
    {
        PlayLayer::update(dt);

        if (OsuFail::active) {
            OsuFail::time += dt;
            OsuFail::applyEffect();

            // Khi kết thúc animation, vẫn giữ active = true nhưng t = 1.0
            // để âm thanh không bị game tự động trả về bình thường trước khi respawn
            if (OsuFail::time >= OsuFail::duration) {
                OsuFail::time = OsuFail::duration; 
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object)
    {
        PlayLayer::destroyPlayer(player, object);

        // Chỉ áp dụng cho player chính
        if (player != m_player1) return;
        if (m_hasCompletedLevel) return;

        // Lấy thời gian fade từ settings (nếu có)
        float fadeSpeed = 0.8f; 
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

    void onQuit()
    {
        OsuFail::reset();
        PlayLayer::onQuit();
    }

    // Đảm bảo âm thanh reset khi thoát hoặc bắt đầu
    bool init(GJGameLevel* level, bool useReplay, bool dontRun)
    {
        OsuFail::reset();
        return PlayLayer::init(level, useReplay, dontRun);
    }
};