'use strict';

const path = require('path');
const bindings = require('bindings')('grid-solver-native');

class GridSolver {
  constructor() {
    this._native = new bindings.GridSolver();
    this._requestId = 0;
    this._pendingReject = null;
  }

  isGridLoaded() {
    return this._native.isGridLoaded();
  }

  isComputing() {
    return this._native.isComputing();
  }

  cancelComputation() {
    this._native.cancelComputation();
    if (this._pendingReject) {
      this._pendingReject(new Error('Computation cancelled by user'));
      this._pendingReject = null;
    }
  }

  loadCDF(filePath) {
    return this._native.loadCDF(filePath);
  }

  solvePowerFlow(config = {}, onProgress = null) {
    const requestId = ++this._requestId;
    const fullConfig = {
      tolerance: config.tolerance || 1e-6,
      maxIterations: config.maxIterations || 50,
      timeoutMs: config.timeoutMs || 30000,
      ...config
    };

    return new Promise((resolve, reject) => {
      this._pendingReject = reject;

      const wrappedProgress = onProgress
        ? (progress) => {
            if (requestId === this._requestId) {
              onProgress(progress);
            }
          }
        : null;

      const promise = this._native.solvePowerFlowAsync(fullConfig, wrappedProgress);

      promise.then((result) => {
        this._pendingReject = null;
        if (requestId === this._requestId) {
          if (result.success) {
            resolve(result.data);
          } else {
            reject(new Error(result.data?.errorMessage || 'Computation failed'));
          }
        } else {
          reject(new Error('Superseded by newer computation request'));
        }
      }).catch((err) => {
        this._pendingReject = null;
        reject(err);
      });
    });
  }

  solveWithModifications(config = {}, genChanges = [], tapChanges = [], onProgress = null) {
    const requestId = ++this._requestId;
    const fullConfig = {
      tolerance: config.tolerance || 1e-6,
      maxIterations: config.maxIterations || 50,
      timeoutMs: config.timeoutMs || 30000,
      ...config
    };

    return new Promise((resolve, reject) => {
      this._pendingReject = reject;

      const wrappedProgress = onProgress
        ? (progress) => {
            if (requestId === this._requestId) {
              onProgress(progress);
            }
          }
        : null;

      const promise = this._native.solveWithModificationsAsync(
        fullConfig, genChanges, tapChanges, wrappedProgress
      );

      promise.then((result) => {
        this._pendingReject = null;
        if (requestId === this._requestId) {
          if (result.success) {
            resolve(result.data);
          } else {
            reject(new Error(result.data?.errorMessage || 'Computation failed'));
          }
        } else {
          reject(new Error('Superseded by newer computation request'));
        }
      }).catch((err) => {
        this._pendingReject = null;
        reject(err);
      });
    });
  }

  solvePowerFlowSync(config = {}) {
    return this._native.solvePowerFlow(config);
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
