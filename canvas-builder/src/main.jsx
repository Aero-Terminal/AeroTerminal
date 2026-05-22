import React, { useEffect } from 'react'
import ReactDOM from 'react-dom/client'
import { Tldraw } from 'tldraw'
import 'tldraw/tldraw.css'

function App() {
  const handleMount = (editor) => {
    window.editor = editor;
    
    // Load initial snapshot from C++ injected global variable
    if (window.initial_canvas_data) {
      try {
        const data = JSON.parse(window.initial_canvas_data);
        editor.loadSnapshot(data);
      } catch (e) {
        console.error("Error loading snapshot:", e);
      }
    }

    // Debounced save back to C++
    let timeoutId = null;
    editor.store.listen((entry) => {
      if (entry.source === 'user') {
        if (timeoutId) clearTimeout(timeoutId);
        timeoutId = setTimeout(() => {
          if (window.save_canvas_data) {
            const snapshot = editor.getSnapshot();
            window.save_canvas_data(JSON.stringify(snapshot));
          }
        }, 200);
      }
    }, { scope: 'document' });
  };

  return (
    <div style={{ position: 'fixed', inset: 0 }}>
      <Tldraw onMount={handleMount} inferDarkMode={true} />
    </div>
  )
}

ReactDOM.createRoot(document.getElementById('root')).render(<App />)
