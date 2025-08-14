// import { useState } from 'react'
import React from 'react';
import './App.css'

import * as Juce from './juce-shim/index.js';

const result = JSON.parse(await Juce.getNativeFunction("get_plugin_state")());
console.log(JSON.stringify(result, null, 2));

// ------------------------------

type KeyValueData = Record<string, string>;

type Props = {
  data: KeyValueData;
};

function KeyValueBox({ data }: Props): React.JSX.Element {
  function capitalize(text: string): string {
    return text.charAt(0).toUpperCase() + text.slice(1);
  }
  return (
    <div className="key-value-container">
      {Object.entries(data).map(([key, value]) => (
        <div className="item" key={key}>
          <span className="key">{capitalize(key)}:</span>
          <span className="value">{value}</span>
        </div>
      ))}
    </div>
  )
}


// ------------------------------

function TopBar({ data }: Props): React.JSX.Element {
  return (
    <div className="top-bar">
      <KeyValueBox data={data} />
    </div>
  )
}

// ------------------------------

function Middle(): React.JSX.Element {
  return (
    <div className="middle-container">
      <h2>Sequencer</h2>
    </div>
  )
}

// ------------------------------

function BottomBar(): React.JSX.Element {
  return (
    <div className="bottom-bar">
      <h2>Bottom Bar</h2>
    </div>
  )
}

// ------------------------------

function SequencerPane(): React.JSX.Element {
  const tb_data = {
    'base frequency': result.base_frequency,
    'Tuning Name': result.tuning_name
  };

  return (
    <div className="sequencer-pane">
      <TopBar data={tb_data} />
      <Middle />
      <BottomBar />
    </div>
  )
}

// ------------------------------

function App(): React.JSX.Element {
  return (
    <div>
      <SequencerPane />
    </div>
  )
}

export default App
