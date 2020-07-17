#include "Application.hpp"
#include "ui/element/GridItem.hpp"
#include "ui/element/ScrollableGrid.hpp"
#include "ui/frame/Artists.hpp"
#include "ui/screen/Home.hpp"

// Number of GridItems per row
#define COLUMNS 3

namespace Frame {
    Artists::Artists(Main::Application * a) : Frame(a) {
        // Remove list + headings (I should redo Frame to avoid this)
        this->removeElement(this->list);
        this->removeElement(this->titleH);
        this->removeElement(this->artistH);
        this->removeElement(this->albumH);
        this->removeElement(this->lengthH);

        // Now prepare this frame
        this->heading->setString("Artists");
        CustomElm::ScrollableGrid * grid = new CustomElm::ScrollableGrid(this->x(), this->y() + 150, this->w() - 10, this->h() - 150, 250, 3);
        grid->setShowScrollBar(true);
        grid->setScrollBarColour(this->app->theme()->muted2());

        // Create items for artists (note: this completely breaks how lists are supposed to be used)
        std::vector<Metadata::Artist> m = this->app->database()->getAllArtistMetadata();
        if (m.size() > 0) {
            for (size_t i = 0; i < m.size(); i++) {
                std::string img = (m[i].imagePath.empty() ? "romfs:/misc/noartist.png" : m[i].imagePath);
                CustomElm::GridItem * l = new CustomElm::GridItem(img);
                l->setMainString(m[i].name);
                std::string str = std::to_string(m[i].albumCount) + (m[i].albumCount == 1 ? " album" : " albums");
                str += " | " + std::to_string(m[i].songCount) + (m[i].songCount == 1 ? " song" : " songs");
                l->setSubString(str);
                l->setDotsColour(this->app->theme()->muted());
                l->setTextColour(this->app->theme()->FG());
                l->setMutedTextColour(this->app->theme()->muted());
                ArtistID id = m[i].ID;
                l->setCallback([this, id](){
                    this->changeFrame(Type::Artist, Action::Push, id);
                });
                l->setMoreCallback([this, id]() {
                    this->createMenu(id);
                });
                grid->addElement(l);
            }

            this->subLength->setHidden(true);
            this->subTotal->setString(std::to_string(m.size()) + (m.size() == 1 ? " artist" : " artists" ));
            this->subTotal->setX(this->x() + 885 - this->subTotal->w());

            this->addElement(grid);
            this->setFocussed(grid);

        // Show message if no artists
        } else {
            grid->setHidden(true);
            this->subLength->setHidden(true);
            this->subTotal->setHidden(true);
            Aether::Text * emptyMsg = new Aether::Text(0, grid->y() + grid->h()*0.4, "No artists found!", 24);
            emptyMsg->setColour(this->app->theme()->FG());
            emptyMsg->setX(this->x() + (this->w() - emptyMsg->w())/2);
            this->addElement(emptyMsg);
        }

        this->menu = nullptr;
    }

    void Artists::createMenu(ArtistID id) {
        // Query database first
        Metadata::Artist m = this->app->database()->getArtistMetadataForID(id);
        if (m.ID < 0) {
            return;
        }

        // Don't create another menu if one exists
        if (this->menu == nullptr) {
            this->menu = new CustomOvl::Menu::Artist();
            this->menu->setPlayAllText("Play All");
            this->menu->setAddToQueueText("Add to Queue");
            this->menu->setAddToPlaylistText("Add to Playlist");
            this->menu->setViewInformationText("View Information");
            this->menu->setBackgroundColour(this->app->theme()->popupBG());
            this->menu->setIconColour(this->app->theme()->muted());
            this->menu->setLineColour(this->app->theme()->muted2());
            this->menu->setMutedTextColour(this->app->theme()->muted());
            this->menu->setTextColour(this->app->theme()->FG());
        }

        // Set artist specific things
        this->menu->setImage(new Aether::Image(0, 0, m.imagePath.empty() ? "romfs:/misc/noartist.png" : m.imagePath));
        this->menu->setName(m.name);
        std::string str = std::to_string(m.albumCount) + (m.albumCount == 1 ? " album" : " albums");
        str += " | " + std::to_string(m.songCount) + (m.songCount == 1 ? " song" : " songs");
        this->menu->setStats(str);

        this->menu->setPlayAllFunc([this, m]() {
            std::vector<Metadata::Song> v = this->app->database()->getSongMetadataForArtist(m.ID);
            std::vector<SongID> ids;
            for (size_t i = 0; i < v.size(); i++) {
                ids.push_back(v[i].ID);
            }
            this->app->sysmodule()->sendSetPlayingFrom(m.name);
            this->app->sysmodule()->sendSetQueue(ids);
            this->app->sysmodule()->sendSetSongIdx(0);
            this->app->sysmodule()->sendSetShuffle(this->app->sysmodule()->shuffleMode());
            this->menu->close();
        });

        this->menu->setAddToQueueFunc([this, m]() {
            std::vector<Metadata::Song> v = this->app->database()->getSongMetadataForArtist(m.ID);
            for (size_t i = 0; i < v.size(); i++) {
                this->app->sysmodule()->sendAddToSubQueue(v[i].ID);
            }
            this->menu->close();
        });
        this->menu->setAddToPlaylistFunc(nullptr);
        this->menu->setViewInformationFunc(nullptr);

        this->app->addOverlay(menu);
    }

    Artists::~Artists() {
        delete this->menu;
    }
};