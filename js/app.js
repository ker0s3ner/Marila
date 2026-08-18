/**
 * Marila Studio Main Application Controller
 */

document.addEventListener('DOMContentLoaded', () => {
  // UI Elements
  const editor = document.getElementById('codeEditor');
  const lineNumbers = document.getElementById('lineNumbers');
  const exampleSelect = document.getElementById('exampleSelect');
  const runBtn = document.getElementById('runBtn');
  const exportBtn = document.getElementById('exportBtn');
  const clearBtn = document.getElementById('clearBtn');
  const copyBtn = document.getElementById('copyBtn');
  const activeFileTypeLabel = document.getElementById('activeFileType');

  const consoleOutput = document.getElementById('consoleOutput');
  const logStatus = document.getElementById('logStatus');

  const previewTabs = document.querySelectorAll('.tab-btn');
  const containers = {
    video: document.getElementById('videoContainer'),
    document: document.getElementById('documentContainer'),
    image: document.getElementById('imageContainer')
  };

  // Instantiate Renderers
  const docRenderer = new DocumentRenderer(document.getElementById('documentPaper'));
  
  const videoRenderer = new VideoRenderer(
    document.getElementById('videoCanvas'),
    document.getElementById('timeScrubber'),
    document.getElementById('timeDisplay'),
    document.getElementById('playPauseBtn'),
    document.getElementById('loopBtn')
  );

  const imageRenderer = new ImageRenderer(
    document.getElementById('imageSvg'),
    {
      gridToggle: document.getElementById('gridToggleBtn'),
      zoomIn: document.getElementById('zoomInBtn'),
      zoomOut: document.getElementById('zoomOutBtn'),
      resetView: document.getElementById('resetViewBtn')
    }
  );

  let currentFileType = 'video';

  // Initialize Lucide Icons
  if (window.lucide) {
    window.lucide.createIcons();
  }

  // Update Line Numbers
  function updateLineNumbers() {
    const lines = editor.value.split('\n').length;
    let numbersHtml = '';
    for (let i = 1; i <= lines; i++) {
      numbersHtml += i + '\n';
    }
    lineNumbers.textContent = numbersHtml;
  }

  editor.addEventListener('input', () => {
    updateLineNumbers();
  });

  // Tab switching
  function switchTab(type) {
    currentFileType = type;
    activeFileTypeLabel.textContent = type;

    previewTabs.forEach(btn => {
      btn.classList.toggle('active', btn.dataset.type === type);
    });

    Object.keys(containers).forEach(key => {
      containers[key].classList.toggle('active', key === type);
    });
  }

  previewTabs.forEach(btn => {
    btn.addEventListener('click', () => {
      switchTab(btn.dataset.type);
    });
  });

  // Example Selector
  exampleSelect.addEventListener('change', (e) => {
    const val = e.target.value;
    if (MARILA_EXAMPLES[val]) {
      editor.value = MARILA_EXAMPLES[val];
      updateLineNumbers();
      compileAndRender();
    }
  });

  // Clear & Copy buttons
  clearBtn.addEventListener('click', () => {
    editor.value = '';
    updateLineNumbers();
  });

  copyBtn.addEventListener('click', () => {
    navigator.clipboard.writeText(editor.value);
    logSuccess('Code copied to clipboard!');
  });

  // Shortcut Ctrl+Enter / Cmd+Enter
  window.addEventListener('keydown', (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
      e.preventDefault();
      compileAndRender();
    }
  });

  runBtn.addEventListener('click', () => compileAndRender());

  // Logging helpers
  function logSuccess(msg) {
    logStatus.textContent = 'OK';
    logStatus.className = 'status-ok';
    consoleOutput.textContent = `[${new Date().toLocaleTimeString()}] SUCCESS: ${msg}`;
  }

  function logError(msg) {
    logStatus.textContent = 'ERROR';
    logStatus.className = 'status-err';
    consoleOutput.textContent = `[${new Date().toLocaleTimeString()}] ERROR: ${msg}`;
  }

  // Compile & Render Routine
  function compileAndRender() {
    const code = editor.value.trim();
    if (!code) {
      logError('Code editor is empty.');
      return;
    }

    try {
      // 1. Lexical Analysis
      const lexer = new Lexer(code);
      const tokens = lexer.tokenize();

      // 2. Parsing AST
      const parser = new Parser(tokens);
      const ast = parser.parse();

      // Switch active tab according to defined file type
      if (ast.fileType && containers[ast.fileType]) {
        switchTab(ast.fileType);
      }

      // 3. Evaluation
      const evaluator = new Evaluator(ast);
      const evalData = evaluator.evaluate();

      // 4. Render to appropriate View
      if (ast.fileType === 'document') {
        docRenderer.render(evalData);
        logSuccess(`Compiled document with ${evalData.nodes.length} section(s).`);
      } else if (ast.fileType === 'video') {
        videoRenderer.load(evalData);
        logSuccess(`Compiled video timeline with ${evalData.animations.length} animation step(s).`);
      } else if (ast.fileType === 'image') {
        imageRenderer.render(evalData);
        logSuccess(`Compiled Euclidean image diagram with ${evalData.elements.length} element(s).`);
      }

    } catch (err) {
      console.error(err);
      logError(err.message || 'Syntax error while parsing Marila code.');
    }
  }

  // Export Output Button
  exportBtn.addEventListener('click', () => {
    if (currentFileType === 'image') {
      const svgData = document.getElementById('imageSvg').outerHTML;
      const blob = new Blob([svgData], { type: 'image/svg+xml;charset=utf-8' });
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = 'marila_diagram.svg';
      link.click();
      logSuccess('Exported SVG diagram!');
    } else if (currentFileType === 'document') {
      window.print();
    } else if (currentFileType === 'video') {
      const canvas = document.getElementById('videoCanvas');
      const image = canvas.toDataURL('image/png');
      const link = document.createElement('a');
      link.href = image;
      link.download = 'marila_video_frame.png';
      link.click();
      logSuccess('Exported video frame PNG!');
    }
  });

  // Initial Load: Set Default Code
  editor.value = MARILA_EXAMPLES.video_morph;
  updateLineNumbers();
  compileAndRender();
});
