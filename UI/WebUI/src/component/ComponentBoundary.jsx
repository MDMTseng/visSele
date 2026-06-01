import React from 'react';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.boundary');

// Granular error boundary for ISOLATING a single panel's render crash.
// Unlike RootErrorBoundary (which takes down the whole app on any throw),
// wrapping a component in <ComponentBoundary name="Canvas"> means a render
// throw inside it shows a small inline fallback while the REST of the UI
// keeps working — so a buggy canvas frame can't kill the property sheet.
//
// Usage:
//   <ComponentBoundary name="Canvas">
//     <CanvasComponent ... />
//   </ComponentBoundary>
//
// Click the inline "重試 / Retry" button to clear the error state and re-mount
// the child subtree. If the bug still fires the boundary catches again.
export default class ComponentBoundary extends React.Component {
  constructor(props) {
    super(props);
    this.state = { error: null, errorKey: 0 };
  }
  static getDerivedStateFromError(error) {
    return { error };
  }
  componentDidCatch(error, info) {
    // log to console.error so the diag ring buffer captures it (initDiag wraps
    // console.error). Include the component name so it's debuggable later.
    log.error(
      `[ComponentBoundary:${this.props.name || 'unnamed'}] caught render crash:`,
      error,
      info && info.componentStack,
    );
  }
  retry = () => {
    // Bump errorKey to force the child to remount under a fresh state.
    this.setState((s) => ({ error: null, errorKey: s.errorKey + 1 }));
  };
  render() {
    if (this.state.error) {
      const name = this.props.name || 'component';
      const msg = String((this.state.error && this.state.error.message) || this.state.error);
      return (
        <div
          className="s WXF veleXY white"
          style={{
            border: '1px dashed #c33',
            padding: '0.4cm',
            minHeight: this.props.fallbackHeight || '2cm',
            background: 'rgba(255,235,235,0.5)',
            flexDirection: 'column',
            textAlign: 'center',
            boxSizing: 'border-box',
          }}
        >
          <div style={{ fontWeight: 'bold', marginBottom: '0.3em', color: '#b00' }}>
            ⚠ [{name}] 發生錯誤 / error
          </div>
          <div
            style={{
              maxWidth: '95%',
              wordBreak: 'break-word',
              opacity: 0.65,
              fontSize: '0.4cm',
              margin: '0.3em 0',
            }}
          >
            {msg}
          </div>
          <button
            onClick={this.retry}
            style={{
              padding: '0.2cm 0.6cm',
              fontSize: '0.4cm',
              borderRadius: '4px',
              cursor: 'pointer',
            }}
          >
            重試 / Retry
          </button>
        </div>
      );
    }
    // errorKey forces remount on retry so the child gets a fresh state.
    return <React.Fragment key={this.state.errorKey}>{this.props.children}</React.Fragment>;
  }
}
