import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import vm from 'node:vm';

const source = readFileSync(new URL('../../src/web/WebTemplatesDashboardAp.cpp', import.meta.url), 'utf8');
const script = source.match(/<script>\r?\n([\s\S]*?)<\/script>/)?.[1];
assert.ok(script);
new vm.Script(script); // Parse the complete embedded dashboard too.
function range(start, end) {
  const a = script.indexOf(start), b = script.indexOf(end, a);
  assert.ok(a >= 0 && b > a);
  return script.slice(a, b);
}
const gasCode = range('const GAS_METRICS =', 'function renderGasGrid(');
const chartCode = range('function chartCardForState(', 'function bindChartPointTooltips(');
const formatCode = range('function isNum(', 'function formatBytes(');
const renderCode = range('function renderCharts(', 'function renderEvents(');

function harness(type, unit, decimals, value) {
  const sensors = { optional_gas_sensor_present:true, optional_gas_type:type,
    optional_gas_unit:unit, optional_gas_decimals:decimals, optional_gas:value };
  const context = vm.createContext({ sensors, stateCache:{sensors},
    thresholds:Object.fromEntries(['voc','nox','hcho','co'].map(k=>[k,{bad:100}])) });
  vm.runInContext(gasCode + chartCode + formatCode + `
    globalThis.metric = buildGasMetrics(sensors).find(m => m.key === 'optional_gas');
    globalThis.chart = chartCardForState({lines:[{key:'optional_gas', digits:1, unit:'ppm'}]});
    globalThis.formatted = fmtValForKey('optional_gas', sensors.optional_gas, metric.digits);
  `, context);
  return context;
}
test('SEN0321 tile and chart retain single ppb values with matching thresholds', () => {
  for (const value of [0, 20, 52, 53, 55, 10000]) {
    const { metric, chart, formatted } = harness('O3','ppb',0,value);
    assert.equal(formatted, String(value));
    assert.equal(metric.unit, 'ppb'); assert.equal(metric.digits, 0);
    assert.equal(metric.thr.good, 50); assert.equal(metric.thr.moderate, 100);
    assert.equal(metric.thr.bad, 500); assert.equal(metric.max, 500);
    assert.equal(chart.unit, 'ppb'); assert.equal(chart.lines[0].digits, 0);
  }
});
test('SEN0472 ppm and oxygen percent profiles retain their existing scale', () => {
  const ozone = harness('O3','ppm',2,0.23);
  assert.equal(ozone.formatted, '0.23'); assert.equal(ozone.metric.thr.good, 0.05);
  assert.equal(ozone.chart.unit, 'ppm');
  const oxygen = harness('O2','%Vol',1,20.9);
  assert.equal(oxygen.formatted, '20.9'); assert.equal(oxygen.chart.unit, '%Vol');
  assert.equal(oxygen.chart.normalRange[0], 19.5);
});

function renderChart({type='O3', unit='ppb', digits=0, key='optional_gas', values=[52,55]} = {}) {
  const context = harness(type, unit, digits, values.at(-1));
  const grid = { innerHTML:'' };
  Object.assign(context, {
    document: { getElementById: () => grid },
    settings: { tempUnit:'C' }, chartGroup:'gas',
    CHART_LAYOUTS: { gas:[{title:'Test', unit, lines:[{key, unit, digits}]}] },
    shouldShowChartCard: () => true,
    formatChartTime: String, esc: String,
    convertChartValueByKey: (_key, value) => value,
    mapSeriesKeyToSensorKey: key => key,
    buildChartSvg: () => '<svg></svg>', bindChartPointTooltips: () => {},
    pressureLabelText: () => 'Pressure', pressureUnitLabel: () => 'inHg', pressureDigits: () => 2,
    chartPayload: { timestamps:values.map((_,i)=>i+1), series:[{key, values}] },
  });
  vm.runInContext(renderCode + 'renderCharts(chartPayload);', context);
  return grid.innerHTML;
}
test('rendered SEN0321 chart min/max use whole ppb, including fractional history means', () => {
  for (const [values, min, max] of [
    [[52,55], '52', '55'], [[52.5,54.5], '53','55'], [[0,10000], '0','10000'],
  ]) {
    const html = renderChart({values});
    assert.ok(html.includes(`<span>min ${min} ppb</span>`), html);
    assert.ok(html.includes(`<span>max ${max} ppb</span>`), html);
  }
  assert.ok(renderChart({values:[null,null]}).includes('<span>min -</span>'));
});
test('rendered min/max keep existing precision for legacy gas, HCHO and pressure', () => {
  assert.ok(renderChart({unit:'ppm',digits:2,values:[0.2,0.3]}).includes('<span>min 0.2 ppm</span>'));
  assert.ok(renderChart({type:'O2',unit:'%Vol',digits:1,values:[20.9,21.2]}).includes('<span>min 20.9 %Vol</span>'));
  assert.ok(renderChart({key:'hcho',values:[52.5,55.5]}).includes('<span>min 52.5 ppb</span>'));
  assert.ok(renderChart({key:'pressure',unit:'inHg',values:[29.92,30.02]}).includes('<span>min 29.92 inHg</span>'));
});
