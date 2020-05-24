#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "Database.hpp"
#include <future>
#include "SongMenu.hpp"
#include "Sysmodule.hpp"
#include "Theme.hpp"

// Forward declaration because cyclic dependency /shrug
namespace Screen {
    class MainScreen;
    class Splash;
};

namespace Main {
    // Enumeration for screens (allows for easy switching)
    enum ScreenID {
        Main,
        Splash
    };

    // The Application class represents the "root" object of the app. It stores/handles all states
    // and objects used through the app
    class Application {
        private:
            // Display object used for rendering
            Aether::Display * display;

            // Screens of the app
            Screen::MainScreen * scMain;
            Screen::Splash * scSplash;

            // Overlays
            CustomOvl::SongMenu * ovlSongMenu;
            void setupSongMenu(SongID);

            // Database object
            Database * database_;

            // Sysmodule object which allows communication
            Sysmodule * sysmodule_;

            // Theme object
            Theme * theme_;

            // Thread which handles sysmodule communication
            std::future<void> sysThread;

        public:
            // Constructor inits Aether, screens + other objects
            Application();

            // Wrapper for display function
            void setHoldDelay(int);

            // Pass an overlay element in order to render
            // Element is not deleted when closed!
            void addOverlay(Aether::Overlay *);

            // Sets up and shows the "SongMenu" overlay using given SongID
            void showSongMenu(SongID);
            void showSongMenu(SongID, size_t);  // Also shows remove from queue given position

            // Pass screen enum to change to it
            void setScreen(ScreenID);
            // Push current screen on stack (i.e. keep in memory)
            void pushScreen();
            // Pop screen from stack
            void popScreen();

            // Returns database pointer
            Database * database();
            // Returns sysmodule pointer
            Sysmodule * sysmodule();
            // Returns theme pointer
            Theme * theme();

            // Handles display loop
            void run();
            // Call to stop display loop
            void exit();

            // Destructor frees memory and quits Aether
            ~Application();
    };
};

#endif