'use strict';

const path = require('path');
const bindings = require('bindings')('grid-solver-native');

class GridSolver {
  constructor() {
    this._native = new bindings.GridSolver();
  }

  isGridLoaded() {
    return this._native.isGridLoaded();
  }

  loadCDF(filePath) {
    return new Promise((resolve, reject) => {
      this._native.loadCDFAsync(filePath, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
  }

  loadCDFSync(filePath) {
    return this._native.loadCDF(filePath);
  }

  solvePowerFlow(config = {}) {
    return new Promise((resolve, reject) => {
      this._native.solvePowerFlowAsync(config, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
  }

  solvePowerFlowSync(config = {}) {
    return this._native.solvePowerFlow(config);
  }

  solveWithModifications(config = {}, genChanges = [], tapChanges = []) {
    return new Promise((resolve, reject) => {
      this._native.solveWithModificationsAsync(config, genChanges, tapChanges, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
  }

  solveWithModificationsSync(config = {}, genChanges = [], tapChanges = []) {
    return this._native.solveWithModifications(config, genChanges, tapChanges);
  }

  getGridInfo() {
    return this._native.getGridInfo();
  }
}

module.exports = {
  GridSolver,
  BusType: {
    SLACK: 0,
    PV: 1,
    PQ: 2
  }
};
