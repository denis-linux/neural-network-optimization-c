#!/usr/bin/env python3
"""Select abstention threshold on validation only; report untouched test afterwards."""
import argparse,collections,json,pathlib

def load(p):
 rows=[]
 for line in p.read_text().splitlines():
  y,pred,score=line.split('\t');rows.append((y,pred,float(score)))
 if not rows:raise ValueError('Empty predictions')
 return rows

def stats(rows,labels,threshold):
 per={};correct=sum(y==p for y,p,_ in rows)
 for label in labels:
  tp=sum(y==p==label for y,p,_ in rows);fp=sum(p==label and y!=label for y,p,_ in rows);fn=sum(y==label and p!=label for y,p,_ in rows)
  per[label]={'support':sum(y==label for y,_,_ in rows),'precision':tp/(tp+fp) if tp+fp else 0,'recall':tp/(tp+fn) if tp+fn else 0,'f1':2*tp/(2*tp+fp+fn) if 2*tp+fp+fn else 0}
 accept=[(y,p,s) for y,p,s in rows if s>=threshold]
 return {'n':len(rows),'correct':correct,'accuracy':correct/len(rows),
 'macro_f1_all_training_classes':sum(x['f1'] for x in per.values())/len(labels),
 'macro_f1_present_classes':sum(x['f1'] for x in per.values() if x['support'])/sum(x['support']>0 for x in per.values()),
 'accepted':len(accept),'coverage':len(accept)/len(rows),'accepted_accuracy':sum(y==p for y,p,_ in accept)/len(accept) if accept else None,'per_class':per}

a=argparse.ArgumentParser();a.add_argument('directory',type=pathlib.Path);a.add_argument('data',type=pathlib.Path);args=a.parse_args();d=args.directory
val=load(d/'validation_predictions.tsv');labels=(args.data/'labels.txt').read_text().splitlines()
# Fixed grid and objective before test is read. Empirical validation target, not a guarantee.
choices=[]
for t in [i/1000 for i in range(1001)]:
 accepted=[r for r in val if r[2]>=t]
 if len(accepted)>=50 and sum(y==p for y,p,_ in accepted)/len(accepted)>=.90:choices.append((len(accepted),-t))
threshold=-max(choices)[1] if choices else 1.000001
(d/'threshold.txt').write_text(str(threshold)+'\n')
report={'policy':'40 epochs fixed in advance; threshold=max validation coverage at >=90% validation accepted accuracy, min 50 accepted; fixed grid step .001; test not used for selection', 'threshold':threshold,'validation':stats(val,labels,threshold)}
test=load(d/'test_predictions.tsv');report['test']=stats(test,labels,threshold)
majority=collections.Counter(line.rsplit('\t',1)[1] for line in (args.data/'train.txt').read_text().splitlines()).most_common(1)[0][0]
report['majority_baseline_test']=sum(y==majority for y,_,_ in test)/len(test)
texts=(args.data/'test.txt').read_text().splitlines()
assert len(texts)==len(test)
errors=[{'text':text.rsplit('\t',1)[0],'expected':y,'predicted':p,'score':s} for text,(y,p,s) in zip(texts,test) if y!=p]
(d/'errors_sample.json').write_text(json.dumps(errors[:30],ensure_ascii=False,indent=2)+'\n')
(d/'metrics.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
for split in ('validation','test'):
 s=report[split];print(split, 'n=',s['n'],'accuracy=',round(s['accuracy'],4),'macro_F1=',round(s['macro_f1_all_training_classes'],4),'coverage=',round(s['coverage'],4),'accepted_accuracy=',s['accepted_accuracy'])
print('threshold',threshold)
