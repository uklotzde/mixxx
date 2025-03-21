# Mixxx + aoide

Instructions for Linux.

## Build and install the _aoide_ executables

```shell
git clone https://gitlab.com/uklotzde/aoide-rs.git
cd aoide-rs
cargo build --profile production -p aoide-websrv
cp target/production/aoide-websrv "${HOME}/bin/"
cargo build --profile production -p aoide-file-collection-app
cp target/production/aoide-file-collection-app "${HOME}/bin/"
```

## Create a new _aoide_ collection

- Run `aoide-file-collection-app`.
- Select a music directory to create a new collection.
- Wait for the media tracker to finish, i.e. when the _Abort_ button below the debug progress output pane changed to _Dismiss_.
- Press the _Dismiss_ button.
- Exit the app.

A new collection is created for each music directory. _aoide_ can manage multiple, independent collections in a single database file.

Re-run the app and start _Synchronize music directory_, whenever the content of the music directory has changed.

The application profile config can be found in `${HOME}/.config/aoide-file-collection-app/aoide_desktop_settings.ron`.

The default location for the _aoide_ SQLite3 database is `${HOME}/.local/share/aoide-file-collection-app/aoide.sqlite`.

The location of the database file could be adjusted in the profile config, e.g. if you want to place your `aoide.sqlite` database file in your _Mixxx_ profile directory `${HOME}/.mixxx`.

## Prepare your _aoide_ collection for _Mixxx_

Run the following command on your aoide database file:

```shell
sqlite3 ${HOME}/.local/share/aoide-file-collection-app/aoide.sqlite "UPDATE collection SET kind='org.mixxx' WHERE collection.kind IS NULL"
```

_Mixxx_ will only show the contents of the first _collection_ where _kind_ is set to `org.mixxx`.

## Build Mixxx

Build the branch `aoide` of this repository according to the build instructions for _Mixxx_.

## Configure Mixxx

- Copy the example file `mixxx/res/aoide/aoide_queries.json` into your _Mixxx_ profile directory `${HOME}/.mixxx` and edit it as desired.
- Run _Mixxx_.
- Navigate to the _aoide_ feature in the side-bar.
- Right click on _Queries_ and select _Load queries..._.
- Select the file `${HOME}/.mixxx/aoide_queries.json`.

The results of your queries should now be available as virtual playlists with the pre-defined filtering and sorting.
