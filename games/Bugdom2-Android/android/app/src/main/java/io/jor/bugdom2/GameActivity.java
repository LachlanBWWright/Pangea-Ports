package io.jor.bugdom2;

import org.libsdl.app.SDLActivity;

public class GameActivity extends SDLActivity {
    @Override
    protected String[] getArguments() {
        if (BuildConfig.LUA_SMOKE) return new String[] { "--level", "0" };
        return super.getArguments();
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "Bugdom2" };
    }
}
