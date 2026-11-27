#!/usr/bin/env python3
import contextlib,importlib.util,io,json,pathlib,tempfile,unittest
SPEC=importlib.util.spec_from_file_location('convert',pathlib.Path(__file__).resolve().parents[1]/'tools/convert_dataset.py');mod=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(mod)
class Conversion(unittest.TestCase):
 def test_massive_split_dedup_slots(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=pathlib.Path(tmp);rows=[]
   for i,(split,text) in enumerate([('train','ПРИВЕТ'),('dev',' привет '),('dev','здравствуй'),('test','ЗДРАВСТВУЙ'),('test','добрый день')]):
    rows.append(dict(id=i,partition=split,utt=text,intent='hello',annot_utt='example-slot'))
   src=p/'source.jsonl';src.write_text('\n'.join(json.dumps(x,ensure_ascii=False) for x in rows))
   with contextlib.redirect_stdout(io.StringIO()):mod.convert(src,p/'out','massive')
   for s in ['train','validation','test']:
    self.assertEqual(len((p/'out'/f'{s}.txt').read_text().splitlines()),1)
   self.assertEqual(json.loads((p/'out/test.jsonl').read_text())['annotated'],'example-slot')
   self.assertNotIn('example-slot',(p/'out/test.txt').read_text())
 def test_clinc_oos(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=pathlib.Path(tmp);src=p/'x.json';src.write_text(json.dumps({'train':[['hi','hello']],'oos_train':[['planet','oos']],'val':[['hey','hello']],'test':[['hello there','hello']],'oos_test':[['universe','oos']]}))
   with contextlib.redirect_stdout(io.StringIO()):mod.convert(src,p/'out','clinc')
   self.assertEqual(len((p/'out/test.txt').read_text().splitlines()),2)
   self.assertIn('universe\toos',(p/'out/test.txt').read_text())
 def test_missing_label_fails(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=pathlib.Path(tmp);src=p/'x.json';src.write_text(json.dumps({'train':[['hi','hello']],'val':[['x','new']],'test':[['y','new']]}))
   with self.assertRaises(ValueError):mod.convert(src,p/'out','clinc')
if __name__=='__main__':unittest.main()
