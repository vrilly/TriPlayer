#include "Application.hpp"
#include "LibraryScanner.hpp"
#include "ui/screen/Splash.hpp"
#include "utils/NX.hpp"
#include "utils/Utils.hpp"

namespace Screen {
    Splash::Splash(Main::Application * a) : Screen() {
        this->app = a;

        // Can only exit on an error
        this->onButtonPress(Aether::Button::B, [this](){
            if (this->fatalError) {
                this->app->exit();
            }
        });
        this->onButtonPress(Aether::Button::PLUS, [this](){
            if (this->fatalError) {
                this->app->exit();
            }
        });
    }

    void Splash::scanLibrary() {
        // Ensure the database is up to date
        this->app->lockDatabase();
        bool ok = this->app->database()->migrate();
        this->app->unlockDatabase();
        if (!ok) {
            this->currentStage = ScanStage::Error;
            return;
        }

        // First create the LibraryScanner object
        this->app->database()->openReadOnly();
        LibraryScanner scanner = LibraryScanner(this->app->database(), "/music");

        // Get files on SD card and analyze what actions need to be taken
        this->currentStage = ScanStage::Files;
        LibraryScanner::Status result = scanner.processFiles();

        // Everything is up to date!
        if (result == LibraryScanner::Status::Done) {
            this->currentStage = ScanStage::Done;
            return;

        // Something went wrong...
        } else if (result == LibraryScanner::Status::ErrDatabase || result == LibraryScanner::Status::ErrUnknown) {
            this->currentStage = ScanStage::Error;
            return;
        }

        // Parse all required files and get their metadata
        if (result != LibraryScanner::Status::DoneRemove) {
            this->currentStage = ScanStage::Metadata;
            result = scanner.processMetadata(this->currentFile, this->totalFiles, this->estRemaining);

            if (result != LibraryScanner::Status::Ok) {
                this->currentStage = ScanStage::Error;
                return;
            }
        }

        // Lock the database for writing and update with metadata
        this->currentStage = ScanStage::Database;
        this->app->sysmodule()->waitReset();
        this->app->lockDatabase();
        result = scanner.updateDatabase();
        this->app->unlockDatabase();
        if (result != LibraryScanner::Status::Ok) {
            this->currentStage = ScanStage::Error;
            return;
        }

        // Now re-lock the database to update for album art
        if (result != LibraryScanner::Status::DoneRemove) {
            this->currentStage = ScanStage::Art;
            this->app->lockDatabase();
            result = scanner.processArt(this->currentFile);
            this->app->unlockDatabase();
            if (result != LibraryScanner::Status::Ok) {
                this->currentStage = ScanStage::Error;
                return;
            }
        }

        this->currentStage = ScanStage::Done;
        return;
    }

    void Splash::setErrorConnect() {
        this->heading->setString("Unable to connect to sysmodule");
        this->heading->setX(640 - this->heading->w()/2);
        this->heading->setHidden(false);
        this->subheading->setString("Press 'Launch' to attempt to start it in the background");
        this->subheading->setX(640 - this->subheading->w()/2);
        this->subheading->setHidden(false);
        this->launch->setHidden(false);
        this->launch->setX(470);
        this->quit->setHidden(false);
        this->quit->setX(650);
        this->setFocused(this->launch);
    }

    void Splash::setErrorVersion() {
        // Indicate wrong version
        this->heading->setString("The sysmodule and application versions do not match");
        this->heading->setX(640 - this->heading->w()/2);
        this->heading->setHidden(false);
        this->subheading->setString("Please ensure that all components of TriPlayer are up to date");
        this->subheading->setX(640 - this->subheading->w()/2);
        this->subheading->setHidden(false);
        this->launch->setHidden(true);
        this->quit->setHidden(false);
        this->quit->setX(640 - this->quit->w()/2);
        this->setFocused(this->quit);
    }

    void Splash::setScanLaunch() {
        // Initialize all variables too
        this->currentFile = 0;
        this->estRemaining = 0;
        this->fatalError = false;
        this->lastFile = 0;
        this->currentStage = ScanStage::Launch;
        this->lastStage = ScanStage::Launch;

        this->animation->setHidden(true);
        this->heading->setHidden(true);
        this->subheading->setHidden(true);
        this->hint->setHidden(true);
        this->progress->setHidden(true);
        this->percent->setHidden(true);
        this->subheading->setHidden(true);
        this->launch->setHidden(true);
        this->quit->setHidden(true);

        // Take action based on sysmodule status
        switch (this->app->sysmodule()->error()) {
            case Sysmodule::Error::None:
                // Start searching for files
                this->currentStage = ScanStage::Files;
                this->future = std::async(std::launch::async, [this](){
                    Utils::NX::setCPUBoost(true);
                    this->scanLibrary();
                    Utils::NX::setCPUBoost(false);
                });
                return;

            case Sysmodule::Error::DifferentVersion:
                this->setErrorVersion();
                this->fatalError = true;
                break;

            default:
                // All other errors represent a connection issue
                this->setErrorConnect();
                this->fatalError = true;
                break;
        }
    }

    void Splash::setScanFiles() {
        this->heading->setString("Preparing your library...");
        this->heading->setX(640 - this->heading->w()/2);
        this->heading->setHidden(false);
        this->animation->setHidden(false);
        this->hint->setHidden(true);
        this->progress->setHidden(true);
        this->percent->setHidden(true);
        this->subheading->setHidden(true);
        this->launch->setHidden(true);
        this->quit->setHidden(true);
    }

    void Splash::setScanMetadata() {
        this->lastFile = 0;
        this->heading->setString("Scanning new files...");
        this->heading->setX(640 - this->heading->w()/2);
        this->subheading->setHidden(false);
        this->subheading->setString("File 0 of 0");
        this->subheading->setX(640 - this->subheading->w()/2);
        this->percent->setHidden(false);
        this->percent->setString("0%");
        this->percent->setY(610 - this->percent->h()/2);
        this->animation->setHidden(false);
        this->animation->setX(this->progress->x() - 60);
        this->progress->setHidden(false);
        this->hint->setHidden(false);
        this->launch->setHidden(true);
        this->quit->setHidden(true);
    }

    void Splash::setScanDatabase() {
        this->heading->setString("Updating database...");
        this->heading->setX(640 - this->heading->w()/2);
        this->subheading->setHidden(true);
        this->animation->setX(620);
        this->progress->setHidden(true);
        this->percent->setHidden(true);
        this->launch->setHidden(true);
        this->quit->setHidden(true);
    }

    void Splash::setScanArt() {
        this->lastFile = 0;
        this->heading->setString("Extracting album art...");
        this->heading->setX(640 - this->heading->w()/2);
        this->subheading->setHidden(false);
        this->subheading->setString("0 albums processed");
        this->subheading->setX(640 - this->subheading->w()/2);
        this->launch->setHidden(true);
        this->quit->setHidden(true);
    }

    void Splash::setScanError() {
        this->heading->setHidden(false);
        this->heading->setString("An unexpected error occurred");
        this->heading->setX(640 - this->heading->w()/2);
        this->subheading->setHidden(false);
        this->subheading->setString("Check the log files for more information");
        this->subheading->setX(640 - this->subheading->w()/2);
        this->animation->setHidden(true);
        this->progress->setHidden(true);
        this->percent->setHidden(true);
        this->hint->setHidden(true);
        this->launch->setHidden(true);
        this->quit->setHidden(false);
        this->quit->setX(640 - this->quit->w()/2);
    }

    void Splash::update(uint32_t dt) {
        Screen::update(dt);

        // Update UI based on stage of scan
        ScanStage stage = this->currentStage;
        if (stage != this->lastStage) {
            switch (stage) {
                case ScanStage::Launch:
                    // Never called
                    break;

                case ScanStage::Files:
                    this->setScanFiles();
                    break;

                case ScanStage::Metadata:
                    this->setScanMetadata();
                    break;

                case ScanStage::Database:
                    this->setScanDatabase();
                    break;

                case ScanStage::Art:
                    this->setScanArt();
                    break;

                case ScanStage::Done:
                    this->animation->setHidden(true);
                    this->hint->setHidden(true);
                    this->heading->setHidden(true);
                    this->app->setScreen(Main::ScreenID::Home);
                    break;

                case ScanStage::Error:
                    this->fatalError = true;
                    this->setScanError();
                    break;
            }

            this->lastStage = stage;
        }

        // Update strings to match current file
        if (stage == ScanStage::Metadata) {
            size_t file = this->currentFile;
            size_t time = this->estRemaining;
            if (file != this->lastFile) {
                std::string tmp = "File " + std::to_string(file) + " of " + std::to_string(this->totalFiles);
                if (time > 0) {
                    tmp += " (~" + Utils::secondsToHMS(time) + " left)";
                }
                this->subheading->setString(tmp);
                this->subheading->setX(640 - this->subheading->w()/2);
                this->progress->setValue(100 * (float)file/(this->totalFiles + 1));
                this->percent->setString(Utils::truncateToDecimalPlace(std::to_string(this->progress->value()), 1) + "%");
                this->lastFile = file;
            }

        // Update strings to indicate number of albums processed
        } else if (stage == ScanStage::Art) {
            size_t album = this->currentFile;
            if (album != this->lastFile) {
                this->subheading->setString(std::to_string(album) + (album == 1 ? " album" : " albums") + " processed");
                this->subheading->setX(640 - this->subheading->w()/2);
            }
        }
    }

    void Splash::onLoad() {
        // Set background
        this->bg = new Aether::Image(0, 0, "romfs:/bg/splash.png");
        this->addElement(this->bg);

        // Add all other elements
        this->version = new Aether::Text(560, 315, "Version " + std::string(VER_STRING), 26);
        this->version->setColour(this->app->theme()->FG());
        this->addElement(this->version);

        this->heading = new Aether::Text(640, 520, "", 26);
        this->heading->setColour(this->app->theme()->FG());
        this->addElement(this->heading);

        this->subheading = new Aether::Text(640, 560, "", 20);
        this->subheading->setColour(this->app->theme()->FG());
        this->addElement(this->subheading);

        this->progress = new Aether::RoundProgressBar(400, 605, 480, 10);
        this->progress->setValue(0.0);
        this->progress->setBackgroundColour(this->app->theme()->muted2());
        this->progress->setForegroundColour(this->app->theme()->accent());
        this->addElement(this->progress);

        this->percent = new Aether::Text(this->progress->x() + this->progress->w() + 20, 610, "", 20);
        this->addElement(this->percent);

        this->animation = new Aether::Animation(620, 600, 40, 20);
        for (size_t i = 1; i <= 50; i++) {
            Aether::Image * im = new Aether::Image(this->animation->x(), this->animation->y(), "romfs:/anim/infload/" + std::to_string(i) + ".png");
            im->setWH(40, 20);
            im->setColour(this->app->theme()->accent());
            this->animation->addElement(im);
        }
        animation->setAnimateSpeed(50);
        this->addElement(this->animation);

        this->hint = new Aether::Text(640, 685, "Please don't quit until the scan is complete!", 18);
        this->hint->setX(640 - this->hint->w()/2);
        this->hint->setColour(this->app->theme()->muted());
        this->addElement(this->hint);

        this->launch = new Aether::BorderButton(0, 610, 160, 60, 2, "Launch", 26, [this]() {
            this->app->sysmodule()->launch();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));   // Wait for the sysmodule to launch
            this->app->sysmodule()->reconnect();
            this->setScanLaunch();
        });
        this->addElement(this->launch);

        this->quit = new Aether::BorderButton(0, this->launch->y(), 160, 60, 2, "Quit", 26, [this]() {
            this->app->exit();
        });
        this->addElement(this->quit);

        // Attempt to launch sysmodule if config option is set true
        if (this->app->sysmodule()->error() == Sysmodule::Error::NotConnected && this->app->config()->autoLaunchService()) {
            this->launch->callback()();
            return;
        }

        this->setScanLaunch();
    }

    void Splash::onUnload() {
        this->removeElement(this->bg);
        this->removeElement(this->version);
        this->removeElement(this->heading);
        this->removeElement(this->subheading);
        this->removeElement(this->progress);
        this->removeElement(this->percent);
        this->removeElement(this->animation);
        this->removeElement(this->hint);
        this->removeElement(this->launch);
        this->removeElement(this->quit);
    }
};