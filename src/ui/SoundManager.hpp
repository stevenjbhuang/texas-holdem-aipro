#pragma once
#include <SFML/Audio.hpp>

namespace poker {

enum class SoundEvent { Deal = 0, Chip = 1, Fold = 2, Win = 3 };

class SoundManager {
public:
    // Loads assets/sounds/{deal,chip,fold,win}.wav. Missing files emit a warning.
    void loadAll();

    // Plays the sound immediately, restarting it if already in progress.
    void post(SoundEvent e);

private:
    sf::SoundBuffer m_buffers[4];
    sf::Sound       m_sounds[4];  // sf::Sound keeps a reference to its buffer
};

} // namespace poker
