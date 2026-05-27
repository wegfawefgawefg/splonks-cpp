package dev.splonks.game;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class SplonksActivity extends SDLActivity {
    private static final String TAG = "SplonksActivity";
    private File projectRoot;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        projectRoot = new File(getFilesDir(), "splonks");
        try {
            extractProjectTree();
        } catch (IOException ex) {
            Log.e(TAG, "Failed to extract packaged project data", ex);
        }
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "main"};
    }

    @Override
    protected String[] getArguments() {
        if (projectRoot == null) {
            projectRoot = new File(getFilesDir(), "splonks");
        }
        return new String[] {"--project-root", projectRoot.getAbsolutePath()};
    }

    private void extractProjectTree() throws IOException {
        if (!projectRoot.exists() && !projectRoot.mkdirs()) {
            throw new IOException("Could not create " + projectRoot);
        }
        final AssetManager assets = getAssets();
        copyAssetPath(assets, "assets", new File(projectRoot, "assets"), true);
        copyAssetPath(assets, "data", new File(projectRoot, "data"), false);
    }

    private void copyAssetPath(
        AssetManager assets,
        String assetPath,
        File destination,
        boolean overwrite
    ) throws IOException {
        final String[] children = assets.list(assetPath);
        if (children != null && children.length > 0) {
            if (!destination.exists() && !destination.mkdirs()) {
                throw new IOException("Could not create " + destination);
            }
            for (String child : children) {
                copyAssetPath(assets, assetPath + "/" + child, new File(destination, child), overwrite);
            }
            return;
        }

        if (destination.exists() && !overwrite) {
            return;
        }
        final File parent = destination.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Could not create " + parent);
        }

        try (InputStream input = assets.open(assetPath);
             FileOutputStream output = new FileOutputStream(destination, false)) {
            byte[] buffer = new byte[64 * 1024];
            int bytesRead;
            while ((bytesRead = input.read(buffer)) != -1) {
                output.write(buffer, 0, bytesRead);
            }
        }
    }
}
