#include "ui/SoundManager.hpp"
#include <iostream>

namespace poker {

static const char* k_paths[4] = {
    "assets/sounds/deal.wav",
    "assets/sounds/chip.wav",
    "assets/sounds/fold.wav",
    "assets/sounds/win.wav",
};

void SoundManager::loadAll()
{
    for (int i = 0; i < 4; ++i) {
        if (!m_buffers[i].loadFromFile(k_paths[i]))
            std::cerr << "Warning: could not load " << k_paths[i] << "\n";
        m_sounds[i].setBuffer(m_buffers[i]);
    }
}

void SoundManager::post(SoundEvent e)
{
    m_sounds[static_cast<int>(e)].play();
}

} // namespace poker
