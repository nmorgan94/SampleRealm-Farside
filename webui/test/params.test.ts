import assert from 'node:assert/strict';
import { test } from 'node:test';
import { formatValue, fromProportion, snap, toProportion } from '@samplerealm/modules/editor/params.ts';
import { info, param } from './engine.ts';

// From the Catch2 snapshot in Tests/FarsideTests.cpp
const pluginText: [id: string, proportion: number, text: string][] = [
  ['roomSize', 0, '0.00'],
  ['roomSize', 0.5, '0.50'],
  ['roomSize', 1, '1.00'],
  ['damping', 0, '0.00'],
  ['damping', 0.5, '0.50'],
  ['damping', 1, '1.00'],
  ['width', 0, '0.00'],
  ['width', 0.5, '0.50'],
  ['width', 1, '1.00'],
  ['mix', 0, '0.00'],
  ['mix', 0.5, '0.50'],
  ['mix', 1, '1.00'],
  ['highpassFreq', 0, '20'],
  ['highpassFreq', 0.5, '900'],
  ['highpassFreq', 1, '5000'],
  ['lowpassFreq', 0, '500'],
  ['lowpassFreq', 0.5, '3947'],
  ['lowpassFreq', 1, '20000'],
  ['drive', 0, '0.00'],
  ['drive', 0.5, '0.50'],
  ['drive', 1, '1.00'],
  ['delaySync', 0, 'Off'],
  ['delaySync', 0.5, 'On'],
  ['delaySync', 1, 'On'],
  ['delayNote', 0, '1/16'],
  ['delayNote', 0.5, '1/8D'],
  ['delayNote', 1, '1/2'],
  ['delayTime', 0, '10'],
  ['delayTime', 0.5, '508'],
  ['delayTime', 1, '2000'],
  ['delayFeedback', 0, '0.00'],
  ['delayFeedback', 0.5, '0.48'],
  ['delayFeedback', 1, '0.95'],
  ['delayLevel', 0, '0.00'],
  ['delayLevel', 0.5, '0.50'],
  ['delayLevel', 1, '1.00'],
  ['duck', 0, '0.00'],
  ['duck', 0.5, '0.50'],
  ['duck', 1, '1.00'],
  ['outputGain', 0, '-24.0'],
  ['outputGain', 0.5, '-6.0'],
  ['outputGain', 1, '12.0'],
  ['reverbType', 0, 'Classic'],
  ['reverbType', 0.5, 'Plate'],
  ['reverbType', 1, 'Hall'],
  ['preDelay', 0, '0'],
  ['preDelay', 0.5, '63'],
  ['preDelay', 1, '250'],
  ['spectrumBypass', 0, 'Off'],
  ['spectrumBypass', 0.5, 'On'],
  ['spectrumBypass', 1, 'On'],
];

test('every parameter has a row in pluginText', () => {
  for (const { id } of info.params) assert.ok(pluginText.some(([row]) => row === id), id);
});

test('every value reads as the plugin shows it, at the same knob position', () => {
  for (const [id, proportion, text] of pluginText) {
    assert.equal(formatValue(param(id), fromProportion(param(id), proportion)), text, `${id} at ${proportion}`);
  }
});

test('the skewed highpass maps knob positions to values and back', () => {
  const highpass = param('highpassFreq');

  assert.equal(fromProportion(highpass, 0.25), 176);
  assert.equal(fromProportion(highpass, 0.75), 2446);
  assert.ok(Math.abs(toProportion(highpass, 1000) - 0.5219) < 0.001);
});

test('values snap to the step', () => {
  assert.equal(snap(param('mix'), 0.333333), 0.33);
});
