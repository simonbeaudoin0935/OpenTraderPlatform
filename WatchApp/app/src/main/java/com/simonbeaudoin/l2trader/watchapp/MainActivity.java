package com.simonbeaudoin.l2trader.watchapp;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

/**
 * Main Activity for L2Trader Watch Companion App
 * 
 * This is a simple Hello World application for Wear OS/Galaxy Watch.
 * It displays a greeting message on the watch face.
 */
public class MainActivity extends Activity {

    private TextView mTextView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mTextView = findViewById(R.id.text);
        mTextView.setText("Hello from L2Trader Watch!");
    }
}
