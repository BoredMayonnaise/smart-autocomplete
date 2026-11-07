'use strict';

const $ = id => document.getElementById(id);
let renderTarget=null;
function canvas(){return renderTarget || $('canvas');}
const editor = $('editor');
const controllers = new Map();
const names = {trie:'Trie · prefix lookup', dp:'Levenshtein · edit distance', kmp:'KMP · exact matching'};
const complexity = {trie:'Expected O(L) lookup + O(k) copy', dp:'O(mn) matrix time and space', kmp:'O(N + M + R) time · O(M) space'};
const metricNotes = {
  trie:'Child lookups count trie edge probes. Results copied counts cached words read from the prefix node. Scan checks are character comparisons in the full-dictionary baseline, so the units differ.',
  dp:'Cells counts matrix cells actually computed. Edits counts reconstructed operations. Distance is the final matrix cell.',
  kmp:'LPS and search counts are character comparisons. Naive is the full baseline for the same text and pattern; preprocessing can make KMP total larger on short inputs.'
};
let mode='trie', view='structure', revision=0, serial=0, dictionaryVersion=0, autoPlay=true;
let analysis=null, selectedSuggestion=-1, trace=null, cursor=0, replay=null, timer=null;
let tickNodes=[];
let analysisTimer=null, searchTimer=null, traceGeneration=0, found=[], matchIndex=-1;
let editorSnapshot='', textSnapshot=editor.value;
let showBranches=false;
const statusNames={empty:'Start typing',exact:'Word found',prefix:'Prefix',misspelled:'Corrections',unknown:'No match',too_long:'Word too long'};
const formatNumber=value=>Number(value).toLocaleString('en-US');

function element(tag, className='', text='') {
  const node=document.createElement(tag);
  if(className) node.className=className;
  if(text!==undefined) node.textContent=text;
  return node;
}
function svgElement(tag, attributes={}, text) {
  const node=document.createElementNS('http://www.w3.org/2000/svg',tag);
  for(const [key,value] of Object.entries(attributes)) node.setAttribute(key,String(value));
  if(text!==undefined) node.textContent=text;
  return node;
}
function message(text='',kind='error') {
  $('notice').textContent=text;$('notice').hidden=!text;$('notice').dataset.kind=kind;
  $('notice').setAttribute('role',kind==='success'?'status':'alert');
}
function traceStatus(text) {if($('trace-status').textContent!==text) $('trace-status').textContent=text;}
function pause() {
  clearTimeout(timer);timer=null;$('play').textContent='▶ Play';
  if(trace) traceStatus(cursor===trace.events.length?'Complete':cursor?'Paused':'Ready');
}
function saveDraft() {
  try {
    sessionStorage.setItem('smartautocomplete.draft',JSON.stringify({...editorData(),mode,source:$('source').value,target:$('target').value,pattern:$('pattern').value}));
  } catch { /* The editor still works when browser storage is unavailable. */ }
}
function restoreDraft() {
  try {
    const draft=JSON.parse(sessionStorage.getItem('smartautocomplete.draft'));
    if(!draft || typeof draft.text!=='string' || draft.text.length>32000) return;
    editor.value=draft.text;
    for(const id of ['source','target','pattern']) if(typeof draft[id]==='string' && draft[id].length<=32000) $(id).value=draft[id];
    if(Number.isInteger(draft.start) && Number.isInteger(draft.end)) editor.setSelectionRange(draft.start,draft.end);
    if(Object.hasOwn(names,draft.mode)) setMode(draft.mode,false);
    textSnapshot=editor.value;
  } catch { /* Invalid or old drafts never block startup. */ }
}
function clearSuggestions() {
  analysis=null;selectedSuggestion=-1;
  for(const id of ['completions','corrections']) $(id).replaceChildren(element('span','no-results',id==='completions'?'Type a word to see suggestions.':'None yet.'));
  $('completion-count').textContent='0';$('active-token').textContent='—';
  $('token-status').textContent='Ready';$('token-status').className='badge';
  $('analysis-metrics').textContent='';
}
function clearFind() {
  found=[];matchIndex=-1;$('find-count').textContent='0 matches';
  $('previous').disabled=true;$('next').disabled=true;highlights();
}
function invalidateTrace() {
  pause();++traceGeneration;controllers.get('trace')?.abort();trace=null;replay=null;cursor=0;
  for(const id of ['play','step','restart','scrub']) $(id).disabled=true;
  $('run').disabled=false;$('scrub').max='0';$('scrub').value='0';
  $('run').setAttribute('aria-busy','false');document.querySelector('.stage').setAttribute('aria-busy','false');traceStatus('Ready');
  $('progress-label').textContent='0 / 0';
  canvas().replaceChildren(element('div','empty','Visualize to inspect this input.'));
  renderStepCard();$('result-summary').textContent='No output yet';$('analytics').replaceChildren(element('span','micro','Run a trace to collect measurements.'));
  $('state-strip').replaceChildren();$('ticks').replaceChildren();tickNodes=[];renderState();
  $('graph-context').hidden=true;
}
function observeVersion(version) {
  if(dictionaryVersion && version<dictionaryVersion) return false;
  if(dictionaryVersion && version!==dictionaryVersion) {invalidateTrace();clearFind();clearSuggestions();}
  dictionaryVersion=version;const versionLabel=$('dictionary-version');if(versionLabel) versionLabel.textContent=`Dictionary v${version}`;
  return true;
}
async function request(path, data, channel, method='POST') {
  controllers.get(channel)?.abort();
  const controller=new AbortController();controllers.set(channel,controller);
  const currentRevision=revision, requestId=`web-${++serial}`;
  try {
    const options={method,signal:controller.signal};
    if(method!=='GET') {options.headers={'Content-Type':'application/json'};options.body=JSON.stringify({...data,revision:currentRevision,request_id:requestId});}
    const response=await fetch(`api/${path}`,options);
    const value=await response.json();
    if(controller.signal.aborted || revision!==currentRevision || controllers.get(channel)!==controller) return null;
    if(!response.ok) throw new Error(value.error?.message || `Request failed (${response.status})`);
    if(method!=='GET' && (value.revision!==currentRevision || value.request_id!==requestId)) return null;
    return value;
  } catch(error) {
    if(controller.signal.aborted || revision!==currentRevision || controllers.get(channel)!==controller) return null;
    throw error;
  }
}
function editorData() {return {text:editor.value,start:editor.selectionStart,end:editor.selectionEnd};}
function suggestions() {return [...document.querySelectorAll('.suggestion')];}
function selectSuggestion(index) {
  const items=suggestions();
  selectedSuggestion=items.length ? (index+items.length)%items.length : -1;
  items.forEach((item,i)=>item.setAttribute('aria-pressed',String(i===selectedSuggestion)));
}
function renderSuggestions(result) {
  analysis=result;selectedSuggestion=-1;
  $('active-token').textContent=result.token.word ? `Editing “${result.token.word}”` : 'Place the caret inside a word';
  $('active-token').title=result.token.word?`Active token: ${result.token.word}. UTF-16 replacement range [${result.token.start}, ${result.token.end}).`:'No active English token';
  $('token-status').textContent=statusNames[result.status] || result.status;$('token-status').className=`badge ${result.status}`;
  $('completion-count').textContent=String(result.completions.length);
  for(const [id,words] of [['completions',result.completions],['corrections',result.corrections]]) {
    const fragment=document.createDocumentFragment();
    for(const candidate of words) {
      const button=element('button','suggestion');button.dataset.word=candidate.word;
      button.setAttribute('aria-pressed','false');
      button.append(element('span','',candidate.word),element('small','',id==='corrections'?`${candidate.distance} ${candidate.distance===1?'edit':'edits'}`:candidate.usage?`used ${candidate.usage}`:'↵'));
      button.setAttribute('aria-label',`Accept ${candidate.word}${id==='corrections'?`, edit distance ${candidate.distance}`:''}`);
      button.addEventListener('click',()=>accept(candidate.word));fragment.append(button);
    }
    if(!words.length) {
      const empty=result.status==='too_long'?`Words support up to ${result.workload.word_limit} code points.`:result.status==='empty'?(id==='completions'?'Type a word to see suggestions.':'None yet.'):id==='completions'?'No completions.':result.status==='exact' || result.status==='prefix'?'No correction needed.':'No nearby words.';
      fragment.append(element('span','no-results',empty));
    }
    $(id).replaceChildren(fragment);
  }
  const m=result.metrics;
  $('analysis-metrics').textContent=m.visited
    ? `Fuzzy: ${formatNumber(m.visited)} nodes · ${formatNumber(m.cells)} cells · ${formatNumber(m.pruned)} pruned rows · ${formatNumber(m.candidates)} eligible words before top eight.`
    : m.lookups?`${formatNumber(m.lookups)} child lookups · full scan: ${formatNumber(m.scan_checks)} character checks.`:'';
}
async function analyze() {
  clearTimeout(analysisTimer);
  try {
    const result=await request('analyze',{...editorData(),threshold:Number($('threshold').value)},'analyze');
    if(!result || !observeVersion(result.dictionary_version)) return null;
    renderSuggestions(result);return result;
  } catch(error) {clearSuggestions();message(error.message);return null;}
}

function editorChanged(force=false) {
  const snapshot=`${editor.selectionStart}:${editor.selectionEnd}:${editor.value}`;
  if(!force && snapshot===editorSnapshot) return;
  editorSnapshot=snapshot;const textChanged=textSnapshot!==editor.value;textSnapshot=editor.value;
  saveDraft();
  ++revision;message();clearSuggestions();controllers.get('analyze')?.abort();controllers.get('accept')?.abort();
  if(textChanged || mode==='trie') invalidateTrace();
  if(textChanged) {clearFind();controllers.get('search')?.abort();}
  clearTimeout(analysisTimer);analysisTimer=setTimeout(analyze,160);
  if(mode==='kmp' && textChanged) {clearTimeout(searchTimer);searchTimer=setTimeout(search,220);}
}
async function accept(word) {
  if(!analysis) return;
  const previous=analysis.token.normalized;
  try {
    const result=await request('accept',{...editorData(),word,dictionary_version:dictionaryVersion},'accept');
    if(!result) return;
    if(!observeVersion(result.dictionary_version)) return;
    editor.value=result.text;
    editor.focus();editor.setSelectionRange(result.caret,result.caret);
    if(mode==='dp') {$('source').value=previous;$('target').value=word;}
    editorChanged(true);await analyze();await runTrace();
  } catch(error) {
    ++revision;clearTimeout(analysisTimer);controllers.get('analyze')?.abort();
    clearSuggestions();invalidateTrace();message(error.message);await refreshDictionary(false);
  }
}
function highlights() {
  const fragment=document.createDocumentFragment();
  const ranges=[];
  for(const match of found) {
    const last=ranges[ranges.length-1];
    if(last && match.utf16_start<=last.end) last.end=Math.max(last.end,match.utf16_end);
    else ranges.push({start:match.utf16_start,end:match.utf16_end});
  }
  let offset=0;
  for(const range of ranges) {
    fragment.append(document.createTextNode(editor.value.slice(offset,range.start)));
    const mark=element('mark','',editor.value.slice(range.start,range.end));fragment.append(mark);offset=range.end;
  }
  fragment.append(document.createTextNode(editor.value.slice(offset)+'\n'));
  $('highlights').replaceChildren(fragment);$('highlights').scrollTop=editor.scrollTop;$('highlights').scrollLeft=editor.scrollLeft;
}
async function search() {
  clearTimeout(searchTimer);
  try {
    const result=await request('search',{text:editor.value,pattern:$('pattern').value},'search');
    if(!result || !observeVersion(result.dictionary_version)) return null;
    found=result.matches;matchIndex=-1;$('find-count').textContent=`${found.length} matches`;
    $('previous').disabled=!found.length;$('next').disabled=!found.length;highlights();return result;
  } catch(error) {clearFind();message(error.message);return null;}
}
function navigateMatch(direction) {
  if(!found.length) return;
  matchIndex=matchIndex<0 ? (direction>0?0:found.length-1) : (matchIndex+direction+found.length)%found.length;
  const match=found[matchIndex];editor.focus();editor.setSelectionRange(match.utf16_start,match.utf16_end);
  $('find-count').textContent=`${matchIndex+1} / ${found.length}`;editorChanged();
}
function setMode(next,run=true) {
  mode=next;++revision;invalidateTrace();message();
  document.querySelectorAll('[data-mode]').forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.mode===mode)));
  document.querySelectorAll('[data-fields]').forEach(section=>{section.hidden=section.dataset.fields!==mode;});
  $('scene-title').textContent=names[mode];$('complexity').textContent=complexity[mode];$('metric-terms').textContent=metricNotes[mode];
  $('graph-context').hidden=!trace || mode!=='trie' || view!=='structure';saveDraft();
  if(mode!=='kmp') clearFind();
  if(run) runTrace();
}
async function runTrace() {
  pause();message();
  let data={mode};
  const currentRevision=revision;
  if(mode==='trie') {
    const result=await analyze();if(!result || currentRevision!==revision) return;
    data.prefix=result.token.normalized;
  } else if(mode==='dp') {data.source=$('source').value;data.target=$('target').value;}
  else {if(!await search() || currentRevision!==revision) return;data.text=editor.value;data.pattern=$('pattern').value;}
  invalidateTrace();const generation=++traceGeneration;$('run').disabled=true;$('run').setAttribute('aria-busy','true');
  document.querySelector('.stage').setAttribute('aria-busy','true');traceStatus('Computing');renderStepCard();
  try {
    const result=await request('trace',data,'trace');
    if(!result || generation!==traceGeneration || !observeVersion(result.dictionary_version)) return;
    trace=result;cursor=0;replay=initialState();buildTicks();
    for(const id of ['play','step','restart','scrub']) $(id).disabled=false;
    $('scrub').max=String(trace.events.length);render();
  } catch(error) {invalidateTrace();message(error.message);}
  finally {if(generation===traceGeneration) {$('run').disabled=false;$('run').setAttribute('aria-busy','false');document.querySelector('.stage').setAttribute('aria-busy','false');}}
}
function initialState() {
  return {counters:{},event:null,previous:null,node:0,visited:new Set([0]),edges:new Set(),missing:false,missingIndex:null,cached:false,
    cells:new Map(),path:new Set(),lps:[],matches:[],i:0,j:0,phase:'initialize',transformed:trace?.inputs.source || '',alignment:false,done:false};
}
function applyEvent(state,event) {
  state.previous=state.event;state.event=event;state.counters=event.counters;
  switch(event.kind) {
    case 'trie_edge_lookup':
      state.node=event.condition?event.other:event.node;
      if(event.condition) {state.visited.add(event.other);state.edges.add(`${event.node}-${event.other}`);}break;
    case 'prefix_missing':state.missing=true;state.missingIndex=event.i;state.node=event.node;break;
    case 'cache_read':state.cached=true;state.node=event.node;break;
    case 'dp_initialize':case 'dp_cell':state.i=event.i;state.j=event.j;state.cells.set(`${event.i}:${event.j}`,event.value);break;
    case 'dp_backtrack':state.i=event.i;state.j=event.j;state.path.add(`${event.i}:${event.j}`);break;
    case 'edit_apply':state.transformed=event.text;state.alignment=true;break;
    case 'lps_compare':state.phase='lps';state.i=event.i;state.j=event.j;break;
    case 'lps_write':state.phase='lps';state.i=event.i;state.j=event.j;state.lps[event.i]=event.value;break;
    case 'lps_fallback':state.i=event.i;state.j=event.value;break;
    case 'search_initialize':state.phase='search';state.i=0;state.j=0;break;
    case 'kmp_compare':case 'kmp_advance':state.i=event.i;state.j=event.j;break;
    case 'kmp_fallback':state.i=event.i;state.j=event.value;break;
    case 'match_found':state.matches.push({start:event.i,end:event.j});break;
    case 'complete':state.done=true;state.alignment=true;break;
  }
}
function seek(value) {
  if(!trace) return;
  const next=Math.max(0,Math.min(trace.events.length,Number(value)));
  if(next!==cursor+1) {replay=initialState();for(let index=0;index<next;index++) applyEvent(replay,trace.events[index]);}
  else applyEvent(replay,trace.events[cursor]);
  cursor=next;render();if(cursor===trace.events.length) pause();
}
function play() {
  if(!trace) return;
  if(timer) {pause();return;}
  if(cursor===trace.events.length) seek(0);
  $('play').textContent='Ⅱ Pause';
  traceStatus('Playing');
  const tick=()=>{if(!trace) return;seek(cursor+1);if(cursor<trace.events.length) timer=setTimeout(tick,400/Number($('speed').value));};
  timer=setTimeout(tick,400/Number($('speed').value));
}
function makeSvg(width,height) {
  const svg=svgElement('svg',{class:'graph',width,height,viewBox:`0 0 ${width} ${height}`,role:'img','aria-label':`${names[mode]} ${view} at event ${cursor}`});
  const defs=svgElement('defs');
  const marker=svgElement('marker',{id:'arrow',viewBox:'0 0 10 10',refX:9,refY:5,markerWidth:6,markerHeight:6,orient:'auto-start-reverse'});
  marker.append(svgElement('path',{d:'M0 0L10 5L0 10',fill:'#8099b7'}));defs.append(marker);svg.append(defs);return svg;
}
function stat(label,value,baseline=false) {
  const tile=element('div',`stat ${baseline?'baseline':''}`);
  tile.append(element('strong','',typeof value==='number'?formatNumber(value):String(value)),element('span','',label));return tile;
}
function buildTicks() {
  const wrap=$('ticks');wrap.replaceChildren();tickNodes=[];
  if(!trace) return;
  const total=trace.events.length,step=Math.max(1,Math.ceil(total/60));
  for(let index=0;index<total;index+=step) {
    const tick=element('span','tick');tick.style.left=`${(index+1)/total*100}%`;
    const label=trace.flow.nodes.find(node=>node.id===trace.events[index].flow)?.label || trace.events[index].kind;
    tick.title=`${index+1}. ${label}`;wrap.append(tick);tickNodes.push({node:tick,index});
  }
}
function updateTicks() {
  for(const {node,index} of tickNodes) node.classList.toggle('passed',index<cursor);
}
function chip(label,value) {
  const item=element('span','state-chip');item.append(element('b','',label),element('em','',String(value)));return item;
}
function renderState() {
  const strip=$('state-strip');strip.replaceChildren();
  if(!trace) {strip.append(chip('Trace','not built'));return;}
  const event=replay.event;
  if(mode==='trie') {
    const prefix=(trace.initial.prefix || []).map(character=>character).join('');
    strip.append(chip('Prefix',prefix || '∅'),chip('Node',replay.node),chip('Depth',Math.max(0,replay.visited.size-1)),chip('Cache',replay.cached?'read':replay.missing?'none':'pending'));
  } else if(mode==='dp') {
    strip.append(chip('i',replay.i),chip('j',replay.j),chip('Cell',event?.kind==='dp_cell'?event.value:'—'),chip('Distance',replay.done?trace.result.distance:'—'));
  } else {
    strip.append(chip('Phase',replay.phase==='lps'?'LPS':replay.phase==='search'?'search':'start'),chip('i',replay.i),chip('j',replay.j),chip('Shift',replay.phase==='search'?`i − j = ${replay.i-replay.j}`:'—'),chip('Matches',replay.matches.length));
  }
}
function describeEvent(event) {
  if(!event) return trace ? 'Initial structure. Press Play or Step to follow the computation.' : 'Press Visualize or pick an example to build a trace.';
  const kind=event.kind;
  if(mode==='trie') {
    const prefix=(trace.initial.prefix || []).join('');
    if(kind==='initialize') return `Start at the root and follow "${prefix || '∅'}" one character at a time.`;
    if(kind==='trie_edge_lookup') {
      const character=trace.initial.prefix[event.i] || '?';
      return event.condition
        ? `Lookup ${event.i+1} of ${prefix.length}: edge "${character}" exists at node ${event.node} → node ${event.other}.`
        : `Lookup ${event.i+1} of ${prefix.length}: node ${event.node} has no "${character}" edge. Prefix ends here.`;
    }
    if(kind==='prefix_missing') return `"${prefix}" is not in the trie. Result: 0 completions.`;
    if(kind==='cache_read') {
      const words=trace.result.suggestions.map(candidate=>candidate.word);
      return `Node ${event.node} holds the cached top ${words.length}${words.length?': '+words.join(', '):''}. Copy them without walking their suffixes.`;
    }
    if(kind==='complete') {
      const count=trace.result.suggestions.length;
      return count ? `${count} ${count===1?'suggestion is':'suggestions are'} ready to use. Choose a completion beside the editor to accept it.` : 'No completions for this prefix. Try a shorter prefix or edit the word list.';
    }
  }
  if(mode==='dp') {
    const source=(trace.initial.source || []),target=(trace.initial.target || []);
    if(kind==='initialize') return `Create a ${source.length+1} × ${target.length+1} matrix and initialize the empty-string borders.`;
    if(kind==='dp_initialize') return `Border cell [${event.i}, ${event.j}] = ${event.value}.`;
    if(kind==='dp_cell') {
      const diagonal=event.condition?'match':'replace';
      const direction={diagonal,up:'delete',left:'insert'}[event.text] || event.text;
      return `Compare "${source[event.i-1]}" with "${target[event.j-1]}" → ${event.condition?'equal':'different'}. min(${diagonal} ${event.diagonal}, delete ${event.up}, insert ${event.left}) = ${event.value}; chose ${direction}.`;
    }
    if(kind==='dp_backtrack') return `Follow the optimal predecessor through [${event.i}, ${event.j}] with distance ${event.value}.`;
    if(kind==='edit_apply') return `Apply edit ${event.value}; source becomes "${event.text}".`;
    if(kind==='complete') return `Final distance ${trace.result.distance} with ${trace.result.edits.length} reconstructed ${trace.result.edits.length===1?'edit':'edits'}.`;
  }
  if(mode==='kmp') {
    const text=(trace.initial.text || []),pattern=(trace.initial.pattern || []);
    if(kind==='initialize') return 'Build the longest-prefix-suffix table before searching.';
    if(kind==='lps_compare') return `i = ${event.i} · j = ${event.j} · compare pattern[${event.i}] "${pattern[event.i]}" with pattern[${event.j}] "${pattern[event.j]}" → ${event.condition?'equal':'mismatch'}.`;
    if(kind==='lps_write') return `i = ${event.i} · j = ${event.j} · write LPS[${event.i}] = ${event.value}.`;
    if(kind==='lps_fallback') return `i = ${event.i} · j = ${event.j} → ${event.value} · mismatch; reuse LPS[${event.j-1}]. The text pointer is not involved.`;
    if(kind==='search_initialize') return 'LPS ready. Start the search at i = 0 · j = 0.';
    if(kind==='kmp_compare') return `i = ${event.i} · j = ${event.j} · text[${event.i}] "${text[event.i]}" vs pattern[${event.j}] "${pattern[event.j]}" → ${event.condition?'equal':'mismatch'}.`;
    if(kind==='kmp_advance') return `i = ${event.i} · j = ${event.j} · advance the matching pointers.`;
    if(kind==='kmp_fallback') return `i = ${event.i} · j = ${event.j} → ${event.value} · keep text[${event.i}]; fall back only the pattern pointer.`;
    if(kind==='match_found') return `Match at code points [${event.i}, ${event.j}) · ${replay.matches.length} found so far.`;
    if(kind==='complete') {
      const count=trace.result.matches.length;
      return count ? `${count} ${count===1?'match':'matches'} found. Use the editor’s previous and next controls to move between matches.` : 'No exact matches. Try another pattern; search is case-sensitive.';
    }
  }
  return kind.replaceAll('_',' ');
}
function renderStepCard(event=null) {
  const busy=$('run').getAttribute('aria-busy')==='true' && !trace;
  const titles={initialize:'Set up the computation',trie_edge_lookup:'Follow a character',prefix_missing:'Prefix not found',cache_read:'Read cached suggestions',
    dp_initialize:'Set the matrix borders',dp_cell:'Choose the lowest cost',dp_backtrack:'Follow the optimal path',edit_apply:'Apply the next edit',
    lps_compare:'Compare pattern characters',lps_write:'Store a prefix length',lps_fallback:'Reuse a shorter prefix',search_initialize:'Begin the text search',
    kmp_compare:'Compare text and pattern',kmp_advance:'Advance the pointers',kmp_fallback:'Shift the pattern pointer',match_found:'Record a match',
    complete:{trie:replay?.missing?'Prefix not found':'Lookup complete',dp:'Distance calculated',kmp:'Search complete'}[mode]};
  $('step-card').dataset.state=replay?.missing?'missing':replay?.done?'complete':event?'active':'ready';
  $('step-phase').textContent={trie:'Prefix lookup',dp:'Edit distance',kmp:replay?.phase==='lps'?'LPS preprocessing':'Text search'}[mode];
  $('step-number').textContent=busy?'··':String(cursor).padStart(2,'0');
  $('operation').textContent=event ? titles[event.kind] || trace.flow.nodes.find(node=>node.id===event.flow)?.label || event.kind.replaceAll('_',' ') : busy?'Preparing replay':trace?'Ready to replay':'Ready to explore';
  $('detail').textContent=busy?'Preparing the computation and its recorded steps.':describeEvent(event);
  $('step-position').textContent=trace?`${cursor} of ${trace.events.length} events`:busy?'Building trace':'No trace yet';
  $('step-outcome').textContent=replay?.missing?'No branch':replay?.done?'Complete':busy?'Loading':event?'In progress':'Ready';
}
function renderPrefixProgress() {
  const prefix=trace.initial.prefix || [];
  const visited=Math.max(0,Math.min(prefix.length,replay.visited.size-1));
  const wrap=element('div','prefix-progress');
  wrap.setAttribute('aria-label',`Prefix ${prefix.map(character=>character).join('')}, ${visited} of ${prefix.length} characters matched`);
  prefix.forEach((character,index)=>{
    let classes='prefix-letter';
    if(index<visited) classes+=' visited';
    else if(replay.missing && replay.missingIndex===index) classes+=' missing';
    else if(index===visited) classes+=' current';
    wrap.append(element('span',classes,character));
  });
  return wrap;
}
function renderTrie() {
  const graph=trace.initial;
  const visible=graph.nodes.filter(node=>showBranches || node.focus!==false);
  const nodes=new Map(visible.map(node=>[node.id,node]));
  const edges=graph.edges.filter(edge=>nodes.has(edge.from) && nodes.has(edge.to));
  const children=new Map();
  for(const edge of edges) {if(!children.has(edge.from)) children.set(edge.from,[]);children.get(edge.from).push(edge.to);}
  const maxDepth=Math.max(0,...visible.map(node=>node.depth));
  const canvasWidth=canvas().clientWidth || 760;
  const span=Math.max(50,Math.min(76,(canvasWidth-82)/Math.max(1,maxDepth)));
  let leaf=0;
  const positions=new Map();
  function position(id) {
    const childIds=children.get(id) || [];
    childIds.sort((a,b)=>a-b);
    const ys=childIds.map(position);
    const y=ys.length ? (ys[0]+ys[ys.length-1])/2 : 32+leaf++*46;
    const node=nodes.get(id);positions.set(id,{x:32+node.depth*span,y});return y;
  }
  position(0);
  canvas().append(renderPrefixProgress());
  const graphWidth=Math.max(canvasWidth-32,maxDepth*span+70);
  const svg=makeSvg(graphWidth,Math.max(250,leaf*46+28));
  const current=replay.event;
  for(const edge of edges) {
    const from=positions.get(edge.from),to=positions.get(edge.to),key=`${edge.from}-${edge.to}`;
    const active=current?.kind==='trie_edge_lookup' && current.condition && current.node===edge.from && current.other===edge.to;
    svg.append(svgElement('path',{class:`edge ${replay.edges.has(key)?'visited':''} ${active?'current':''}`,d:`M${from.x+15} ${from.y}C${from.x+span*.48} ${from.y},${to.x-span*.48} ${to.y},${to.x-16} ${to.y}`,'marker-end':'url(#arrow)'}));
    svg.append(svgElement('text',{x:(from.x+to.x)/2,y:(from.y+to.y)/2-5,class:'edge-label','text-anchor':'middle'},edge.character));
  }
  for(const node of visible) {
    const point=positions.get(node.id),active=current && replay.node===node.id;
    svg.append(svgElement('circle',{cx:point.x,cy:point.y,r:15,class:`node ${node.terminal?'terminal':''} ${replay.visited.has(node.id)?'visited':''} ${active?'current':''}`}));
    if(node.terminal) svg.append(svgElement('circle',{cx:point.x,cy:point.y,r:11,fill:'none',stroke:'#88e0b4','stroke-width':1}));
    svg.append(svgElement('text',{x:point.x,y:point.y+4,'text-anchor':'middle'},node.character));
    svg.append(svgElement('text',{x:point.x,y:point.y+26,class:'node-id','text-anchor':'middle'},String(node.id)));
  }
  let missingEvent=current?.kind==='prefix_missing'?current:null;
  if(!missingEvent && current?.kind==='trie_edge_lookup' && current.condition===false) missingEvent=current;
  if(!missingEvent && replay.missing) missingEvent=[...trace.events.slice(0,cursor)].reverse().find(event=>event.kind==='prefix_missing');
  if(missingEvent) {
    const point=positions.get(missingEvent.node);
    const character=trace.initial.prefix[missingEvent.i] || '?';
    if(point) {
      svg.append(svgElement('path',{d:`M${point.x+16} ${point.y}L${point.x+48} ${point.y}`,stroke:'#ff9caa','stroke-width':2,'stroke-dasharray':'4 3'}));
      svg.append(svgElement('text',{x:point.x+32,y:point.y-8,class:'cache-label',fill:'#ff9caa','text-anchor':'middle'},`× ${character}`));
      svg.append(svgElement('circle',{cx:point.x,cy:point.y,r:15,class:'node missing-ring',fill:'none'}));
    }
  }
  if(replay.cached) {
    const endpoint=positions.get(replay.node);
    if(endpoint) svg.append(svgElement('text',{x:endpoint.x,y:endpoint.y-24,class:'cache-label','text-anchor':'middle'},`cache ×${trace.result.suggestions.length}`));
  }
  canvas().append(svg);
  const overflow=graphWidth>canvasWidth-32?' · scroll for full path':'';
  canvas().append(element('div','window-label',`${visible.length} / ${formatNumber(graph.total_nodes)} nodes shown · ${formatNumber(graph.total_nodes-visible.length)} hidden · gray suffixes not visited${overflow}`));
}
function charRow(name,items,indices,classes=[],small=[]) {
  const row=element('div','char-row');row.append(element('span','row-name',name));
  items.forEach((value,index)=>{
    const cell=element('span',`char ${indices?'index':''} ${classes[index] || ''}`,value);
    if(small[index]!==undefined && small[index]!==null) cell.append(element('small','',String(small[index])));
    row.append(cell);
  });return row;
}
function renderKmp() {
  const {text,pattern}=trace.initial;
  const preprocessing=replay.phase==='lps';
  const input=preprocessing?pattern:text;
  const capacity=Math.max(8,Math.min(22,Math.floor(((canvas().clientWidth || 760)-88)/38)));
  const windowFor=(focus,length)=>{
    const start=Math.max(0,Math.min(focus-Math.floor(capacity/2),Math.max(0,length-capacity)));
    return [start,Math.min(length,start+capacity)];
  };
  const [start,end]=windowFor(replay.i,input.length),indices=[],characters=[],classes=[];
  const event=replay.event;
  for(let index=start;index<end;index++) {
    indices.push(String(index));characters.push(input[index]);
    let name='';
    if(!preprocessing && index===replay.i) name+=' text-pointer';
    if(preprocessing && index===replay.j) name+=' pattern-pointer';
    if(index===replay.i && preprocessing) name+=' active';
    if(!preprocessing && replay.matches.some(match=>index>=match.start && index<match.end)) name+=' matched';
    if(event?.kind===(preprocessing?'lps_compare':'kmp_compare') && (index===event.i || (preprocessing && index===event.j))) name+=event.condition?' equal':' mismatch';
    classes.push(name);
  }
  const hidden=Math.max(0,input.length-(end-start));
  const label=preprocessing?'LPS preprocessing':'Text search';
  const shift=preprocessing?'':` · shift i − j = ${replay.i-replay.j}`;
  canvas().append(element('div','window-label',`${label} · window [${start}, ${end}) / ${input.length}${shift} · ${hidden} hidden`));
  canvas().append(charRow('Index',indices,true),charRow(preprocessing?'Pattern':'Text',characters,false,classes));
  if(!preprocessing) {
    const alignment=replay.i-replay.j,items=[],small=[],names=[];
    for(let index=start;index<end;index++) {
      const j=index-alignment,valid=j>=0 && j<pattern.length;
      items.push(valid?pattern[j]:'');small.push(valid?j:null);
      let name=!valid?'blank':'';
      if(index===replay.i) name+=' pattern-pointer';
      if(event?.kind==='kmp_compare' && index===event.i) name+=event.condition?' equal':' mismatch';
      names.push(name);
    }
    canvas().append(charRow('Pattern',items,false,names,small));
  }
  const [lpsStart,lpsEnd]=windowFor(replay.j,pattern.length),lps=[],patternChars=[],lpsIndices=[];
  for(let index=lpsStart;index<lpsEnd;index++) {lps.push(replay.lps[index]===undefined?'·':String(replay.lps[index]));patternChars.push(pattern[index]);lpsIndices.push(String(index));}
  canvas().append(element('div','window-label',`LPS window [${lpsStart}, ${lpsEnd}) / ${pattern.length}`),charRow('Index',lpsIndices,true),charRow('Pattern',patternChars,false),charRow('LPS',lps,false,lps.map(()=> 'lps')));
}
function renderDp() {
  const {source,target}=trace.initial;
  const current=replay.event;
  if(current?.kind==='dp_cell') {
    const choices=element('div','cost-choices');
    for(const [name,value,chosen] of [['Delete',current.up,'up'],[current.condition?'Match':'Replace',current.diagonal,'diagonal'],['Insert',current.left,'left']]) {
      const card=element('div',`cost-choice ${current.text===chosen?'chosen':''}`);
      card.append(element('strong','',String(value)),element('span','',name));choices.append(card);
    }
    canvas().append(choices);
  }
  const table=element('table','matrix');table.setAttribute('aria-label','Edit-distance matrix');
  const head=element('thead'),row=element('tr');
  for(const value of ['', '∅', ...target]) {const th=element('th','',value);th.scope='col';row.append(th);}head.append(row);table.append(head);
  const body=element('tbody');
  for(let i=0;i<=source.length;i++) {
    const row=element('tr');const label=element('th','',i?source[i-1]:'∅');label.scope='row';row.append(label);
    for(let j=0;j<=target.length;j++) {
      const key=`${i}:${j}`,value=replay.cells.get(key);let name=value===undefined?'pending':'';
      if(replay.path.has(key)) name+=' path';
      if(current?.kind==='dp_cell') {
        if((i===current.i-1 && j===current.j-1) || (i===current.i-1 && j===current.j) || (i===current.i && j===current.j-1)) name+=' predecessor';
        if(i===current.i && j===current.j) name+=' chosen';
      }
      if(current && current.kind.startsWith('dp_') && i===current.i && j===current.j) name+=' current';
      row.append(element('td',name,value===undefined?'·':String(value)));
    }
    body.append(row);
  }
  table.append(body);canvas().append(table);
  if(replay.alignment) {
    canvas().append(element('div','alignment',trace.result.aligned_source.map(c=>c || '−').join(' ')+'\n'+trace.result.aligned_target.map(c=>c || '−').join(' ')));
    canvas().append(element('div','transformed',replay.transformed || '∅'));
  }
}
function renderFlow() {
  const graph=trace.flow,positions=new Map();
  const canvasHeight=canvas().clientHeight || 420;
  const gap=Math.max(36,Math.min(60,(canvasHeight-105)/Math.max(1,graph.nodes.length-1)));
  const cx=170,width=Math.max(430,Math.min(620,(canvas().clientWidth || 560)-32));
  graph.nodes.forEach((node,index)=>positions.set(node.id,{x:cx,y:32+index*gap,index}));
  const svg=makeSvg(width,Math.max(220,56+(graph.nodes.length-1)*gap));
  for(const edge of graph.edges) {
    const from=positions.get(edge.from),to=positions.get(edge.to),forward=to.index===from.index+1;
    const current=replay.previous?.flow===edge.from && replay.event?.flow===edge.to && (edge.label==='' || edge.label===(replay.previous.condition?'True':'False'));
    let path,labelX=cx+10,labelY=(from.y+to.y)/2;
    const parallel=graph.edges.filter(candidate=>candidate.from===edge.from && candidate.to===edge.to).length>1;
    if(edge.from===edge.to) {
      const bend=edge.label==='False'?24:62;
      path=`M${cx-14} ${from.y-8}C${bend} ${from.y-52},${bend} ${from.y+52},${cx-14} ${from.y+8}`;
      labelX=edge.label==='False'?18:64;labelY=from.y;
    } else if(forward && !parallel) path=`M${cx} ${from.y+16}L${cx} ${to.y-17}`;
    else if(forward) {
      const bend=edge.label==='False'?105:225;
      path=`M${cx} ${from.y+16}C${bend} ${from.y+30},${bend} ${to.y-30},${cx} ${to.y-17}`;
      labelX=bend-8;
    } else {
      const bend=Math.max(20,120-Math.abs(from.index-to.index)*8-(edge.label==='False'?16:0));
      path=`M${cx-16} ${from.y}C${bend} ${from.y},${bend} ${to.y},${cx-16} ${to.y}`;
      labelX=bend;labelY+=5;
    }
    svg.append(svgElement('path',{d:path,class:`edge ${current?'current':''}`,'marker-end':'url(#arrow)'}));
    if(edge.label) svg.append(svgElement('text',{x:labelX,y:labelY,class:'branch-label'},edge.label));
  }
  for(const node of graph.nodes) {
    const point=positions.get(node.id),active=replay.event?.flow===node.id;
    svg.append(svgElement('circle',{cx:point.x,cy:point.y,r:16,class:`node ${active?'current':''}`}));
    svg.append(svgElement('text',{x:point.x,y:point.y+5,'text-anchor':'middle'},String(point.index)));
    svg.append(svgElement('text',{x:cx+28,y:point.y+4},node.label));
  }
  canvas().append(element('div','window-label',graph.scope),svg);
}
function bar(label,value,maximum,baseline=false) {
  const row=element('div','bar-row'),track=element('div','track'),fill=element('div',`bar ${baseline?'baseline':''}`);
  fill.style.width=`${maximum ? value/maximum*100:0}%`;track.append(fill);row.append(element('span','',label),track,element('strong','',String(value)));return row;
}
function renderAnalytics() {
  const metrics=replay.counters;
  $('analytics').replaceChildren();
  if(mode==='kmp') {
    const total=(metrics.lps || 0)+(metrics.search || 0),baseline=trace.metrics.naive,max=Math.max(trace.metrics.lps+trace.metrics.search,baseline);
    $('analytics').append(bar('KMP',total,max),bar('Naive',baseline,max,true),element('span','micro',`${metrics.lps || 0} preprocessing + ${metrics.search || 0} search = ${total} comparisons · naive: full baseline`));
    const difference=trace.result.comparison.difference;
    const comparison=difference===0?'Same comparison count.':`KMP uses ${formatNumber(Math.abs(difference))} ${difference<0?'fewer':'more'} comparisons.`;
    $('analytics').append(element('span','micro',replay.done?comparison:'KMP count is partial; compare totals at completion.'));
    $('analytics').append(element('span','micro',`${trace.workload.text_points} text / ${trace.workload.pattern_points} pattern code points · ${metrics.matches || 0} matches · ${metrics.search_fallbacks || 0} search fallbacks`));
  } else {
    const grid=element('div','stat-grid');
    if(mode==='trie') grid.append(stat('Child lookups',metrics.lookups || 0),stat('Results copied',metrics.copied || 0),stat('Scan checks',trace.metrics.scan_checks,true));
    else grid.append(stat('Cells',metrics.cells || 0),stat('Edits',metrics.edits || 0),stat('Distance',replay.done?trace.result.distance:'—'));
    $('analytics').append(grid);
    if(mode==='trie') $('analytics').append(element('span','micro',`${formatNumber(trace.metrics.scan_words)} words scanned · ${formatNumber(trace.workload.trie_nodes)} stored nodes. Lookups and scan checks use different units.`));
    else $('analytics').append(element('span','micro',`${trace.workload.source_points} source × ${trace.workload.target_points} target code points · cells exclude initialized borders.`));
  }
}
function render() {
  if(!trace || !replay) return;
  const outer=$('canvas');
  const previousScroll={left:outer.scrollLeft,top:outer.scrollTop};
  outer.replaceChildren();
  if(view==='split') {
    const split=element('div','split-canvas');
    const left=element('div','split-pane');const right=element('div','split-pane');
    split.append(left,right);outer.append(split);
    renderTarget=left; if(mode==='trie') renderTrie(); else if(mode==='dp') renderDp(); else renderKmp();
    renderTarget=right; renderFlow();
    renderTarget=null;
  } else {
    renderTarget=outer;
    if(view==='flow') renderFlow(); else if(mode==='trie') renderTrie(); else if(mode==='dp') renderDp(); else renderKmp();
    renderTarget=null;
    outer.scrollLeft=previousScroll.left;outer.scrollTop=previousScroll.top;
  }
  const current=replay.event;
  $('graph-context').hidden=mode!=='trie' || view!=='structure';
  traceStatus(replay.done?'Complete':timer?'Playing':cursor?'Paused':'Ready');
  renderStepCard(current);$('progress-label').textContent=`${cursor} / ${trace.events.length}`;$('scrub').value=String(cursor);
  $('scrub').setAttribute('aria-valuetext',`${cursor} of ${trace.events.length} events`);
  if(mode==='trie') $('result-summary').textContent=replay.cached?trace.result.suggestions.map(word=>word.word).join(' · ') || '0 suggestions':replay.missing?'0 suggestions':'—';
  else if(mode==='dp') $('result-summary').textContent=replay.done?`Distance ${trace.result.distance} · ${trace.result.edits.length} ${trace.result.edits.length===1?'edit':'edits'}`:'—';
  else $('result-summary').textContent=`${replay.matches.length} matches${replay.matches.length?' · starts '+replay.matches.map(match=>match.start).join(', '):''} · code-point indices`;
  renderState();renderAnalytics();updateTicks();
  $('step').disabled=cursor===trace.events.length;
}
async function refreshDictionary(open=true) {
  try {
    const result=await request('dictionary',{},'dictionary','GET');if(!result || !observeVersion(result.dictionary_version)) return;
    $('word-count').textContent=formatNumber(result.count);$('dictionary-words').value=result.words.join('\n');
    $('dictionary-limits').textContent=`${formatNumber(result.count)} / ${formatNumber(result.limits.words)} words · ${result.limits.word} code points / word · English letters, internal ' and -`;
    $('dictionary-error').textContent='';if(open) $('dictionary-dialog').showModal();
  } catch(error) {message(error.message);}
}
async function replaceDictionary() {
  $('save-dictionary').disabled=true;$('dictionary-error').textContent='';
  try {
    const words=$('dictionary-words').value.split(/\r?\n/).map(word=>word.trim()).filter(Boolean);
    const result=await request('dictionary',{words,dictionary_version:dictionaryVersion},'replace','PUT');
    if(!result) return;
    if(!observeVersion(result.dictionary_version)) return;
    $('word-count').textContent=formatNumber(result.count);$('dictionary-dialog').close();
    editorChanged(true);message('Dictionary updated.','success');await analyze();
  } catch(error) {$('dictionary-error').textContent=error.message;}
  finally {$('save-dictionary').disabled=false;}
}
document.querySelectorAll('[data-mode]').forEach(button=>button.addEventListener('click',()=>{$('example').value='';setMode(button.dataset.mode);}));
document.querySelectorAll('[data-view]').forEach(button=>button.addEventListener('click',()=>{
  view=button.dataset.view;document.querySelectorAll('[data-view]').forEach(item=>item.setAttribute('aria-pressed',String(item===button)));
  $('graph-context').hidden=!trace || mode!=='trie' || view!=='structure';render();
}));
function loadExample(fixture) {
  switch(fixture) {
    case 'prefix':editor.value='algor';setMode('trie',false);break;
    case 'insert':editor.value='programing';$('source').value='programing';$('target').value='programming';setMode('dp',false);break;
    case 'transpose':editor.value='recieve';$('source').value='recieve';$('target').value='receive';setMode('dp',false);break;
    case 'distance':editor.value='kitten';$('source').value='kitten';$('target').value='sitting';setMode('dp',false);break;
    case 'overlap':editor.value='banana bandana banana';$('pattern').value='ana';setMode('kmp',false);break;
    case 'stress':editor.value='a'.repeat(36)+'b';$('pattern').value='aaaaab';setMode('kmp',false);break;
  }
  editor.setSelectionRange(editor.value.length,editor.value.length);editorChanged(true);runTrace();
}
$('example').addEventListener('change',()=>{if($('example').value) loadExample($('example').value);});
$('graph-context').addEventListener('click',()=>{
  showBranches=!showBranches;$('graph-context').setAttribute('aria-pressed',String(showBranches));
  $('graph-context').textContent=showBranches?'Focus path':'Show branches';canvas().scrollTop=0;render();
});
for(const event of ['input','click','select','keyup']) editor.addEventListener(event,()=>{if(event==='input') $('example').value='';editorChanged();});
editor.addEventListener('scroll',()=>{$('highlights').scrollTop=editor.scrollTop;$('highlights').scrollLeft=editor.scrollLeft;});
editor.addEventListener('keydown',event=>{
  if(event.key==='ArrowDown' && suggestions().length) {event.preventDefault();selectSuggestion(selectedSuggestion+1);}
  if(event.key==='ArrowUp' && suggestions().length) {event.preventDefault();selectSuggestion(selectedSuggestion<0?suggestions().length-1:selectedSuggestion-1);}
  if((event.key==='Tab' || event.key==='Enter') && selectedSuggestion>=0) {event.preventDefault();accept(suggestions()[selectedSuggestion].dataset.word);}
  if(event.key==='Escape') {selectedSuggestion=-1;suggestions().forEach(item=>item.setAttribute('aria-pressed','false'));}
});
for(const id of ['source','target','pattern']) $(id).addEventListener('input',()=>{
  $('example').value='';
  ++revision;invalidateTrace();message();
  saveDraft();
  if(id==='pattern') {clearFind();clearTimeout(searchTimer);searchTimer=setTimeout(search,180);}
});
$('threshold').addEventListener('change',()=>{++revision;clearSuggestions();analyze();});
$('run').addEventListener('click',async()=>{await runTrace();if(autoPlay) play();});$('play').addEventListener('click',play);
$('step').addEventListener('click',()=>{pause();seek(cursor+1);});$('restart').addEventListener('click',()=>{pause();seek(0);});
$('scrub').addEventListener('input',()=>{pause();seek($('scrub').value);});
$('speed').addEventListener('change',()=>{if(timer) {pause();play();}});
$('previous').addEventListener('click',()=>navigateMatch(-1));$('next').addEventListener('click',()=>navigateMatch(1));

document.addEventListener('visibilitychange',()=>{if(document.hidden) pause();});
restoreDraft();clearSuggestions();clearFind();
$('metric-terms').textContent=metricNotes[mode];renderState();
let resizeTimer=null;
addEventListener('resize',()=>{clearTimeout(resizeTimer);resizeTimer=setTimeout(()=>{if(trace) render();},160);});
(async()=>{editorSnapshot=`${editor.selectionStart}:${editor.selectionEnd}:${editor.value}`;if(mode!=='trie') await analyze();await runTrace();})();
