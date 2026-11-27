#!/usr/bin/env python3
"""Convert MASSIVE JSONL or CLINC150 JSON to TinyTalk TSV, preserving splits.
Python standard library only. Slot annotations are retained, never fed as input.
"""
import argparse, collections, hashlib, json, pathlib, re, unicodedata

def canonical(s):
    return ' '.join(unicodedata.normalize('NFKC', s).lower().replace('ё','е').split())

def convert(source, out, kind):
    buckets = {s: [] for s in ('train', 'validation', 'test')}
    if kind == 'massive':
        for line in source.read_text(encoding='utf-8').splitlines():
            if not line.strip(): continue
            x = json.loads(line)
            split = {'dev':'validation','train':'train','test':'test'}.get(x['partition'])
            if split is None: raise ValueError('Unknown partition: '+str(x['partition']))
            buckets[split].append(dict(id=x['id'], text=x['utt'], label=x['intent'],
                annotated=x.get('annot_utt'), locale=x.get('locale'), scenario=x.get('scenario')))
    else:
        data=json.loads(source.read_text(encoding='utf-8'))
        for name, rows in data.items():
            split={'train':'train','val':'validation','test':'test','oos_train':'train','oos_val':'validation','oos_test':'test'}.get(name)
            if split is None: continue
            for i,row in enumerate(rows):
                text,label=row
                buckets[split].append(dict(id=f'{name}:{i}',text=text,label=label))
    out.mkdir(parents=True,exist_ok=True)
    report={'format':kind,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'policy':'official split; remove repeated NFKC/lower/yo/whitespace text in order train,validation,test; no shuffle across splits', 'splits':{}}
    seen=set(); labels=set()
    for split,rows in buckets.items():
        kept=[];dropped=0
        for x in rows:
            text=x['text'];label=x['label']
            if not isinstance(text,str) or not isinstance(label,str):raise ValueError('Text and label must be strings')
            if not re.fullmatch(r'[A-Za-z0-9_\-]+',label):raise ValueError('Unsafe/unsupported label: '+label)
            text=' '.join(text.split())
            if not text or len(text)>1000:raise ValueError('Empty or >1000 codepoints: '+str(x['id']))
            key=canonical(text)
            if key in seen:dropped+=1;continue
            seen.add(key);x['text']=text;kept.append(x)
        if split=='train':labels={x['label'] for x in kept}
        if any(x['label'] not in labels for x in kept):raise ValueError('Evaluation label absent from training')
        (out/(split+'.txt')).write_text(''.join(x['text']+'\t'+x['label']+'\n' for x in kept),encoding='utf-8')
        (out/(split+'.jsonl')).write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in kept),encoding='utf-8')
        report['splits'][split]={'source':len(rows),'kept':len(kept),'duplicates_removed':dropped,'classes':len({x['label'] for x in kept})}
    if len(labels)>256 or report['splits']['train']['kept']>20000:raise ValueError('Exceeds TinyTalk capacity (256 labels/20000 examples)')
    for s in buckets:
        if not report['splits'][s]['kept']:raise ValueError('Missing split '+s)
    (out/'labels.txt').write_text('\n'.join(sorted(labels))+'\n')
    (out/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('source',type=pathlib.Path);a.add_argument('out',type=pathlib.Path);a.add_argument('--format',choices=['massive','clinc'],required=True)
    args=a.parse_args();convert(args.source,args.out,args.format)
