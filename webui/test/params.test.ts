import assert from 'node:assert/strict';
import { test } from 'node:test';
import { formatValue, fromProportion, snap, toProportion } from '../src/params.ts';
import { info, param } from './engine.ts';

// From the Catch2 snapshot in Tests/ReverbTests.cpp
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
  ['sendGain', 0, '0.00'],
  ['sendGain', 0.5, '1.00'],
  ['sendGain', 1, '2.00'],
  ['highpassFreq', 0, '20'],
  ['highpassFreq', 0.5, '644'],
  ['highpassFreq', 1, '20000'],
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

  assert.equal(fromProportion(highpass, 0.25), 40);
  assert.equal(fromProportion(highpass, 0.75), 4761);
  assert.ok(Math.abs(toProportion(highpass, 1000) - 0.5472) < 0.001);
});

test('positions past either end clamp to the range', () => {
  const gain = param('sendGain');

  assert.equal(fromProportion(gain, 1.5), 2);
  assert.equal(fromProportion(gain, -0.5), 0);
});

test('values snap to the step, and the toggle to off or on', () => {
  assert.equal(snap(param('mix'), 0.333333), 0.33);
  assert.equal(snap(param('spectrumBypass'), 0.7), 1);
});
