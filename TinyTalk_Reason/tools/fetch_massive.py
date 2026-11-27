#!/usr/bin/env python3
"""Download official MASSIVE 1.1, verify fixed SHA256, extract only Russian + notices."""
import hashlib,json,pathlib,tarfile,urllib.request
ROOT=pathlib.Path(__file__).resolve().parents[1]/'datasets'
URL='https://amazon-massive-nlu-dataset.s3.amazonaws.com/amazon-massive-dataset-1.1.tar.gz'
SHA='4cba5faa11c71437928e17cb1b9b3d8b8e727e7ea363a3a9a8045e19c0491577'
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
ROOT.mkdir(parents=True,exist_ok=True);a=ROOT/'massive-1.1.tar.gz'
if not a.exists():
 temp=a.with_suffix('.download')
 try:
  with urllib.request.urlopen(URL,timeout=60) as r,temp.open('wb') as f:
   size=0
   while True:
    b=r.read(1024*1024)
    if not b:break
    size+=len(b)
    if size>100*1024*1024:raise ValueError('Archive exceeds size limit')
    f.write(b)
  if digest(temp)!=SHA:raise ValueError('Upstream checksum changed; inspect source before updating pin')
  temp.replace(a)
 finally:
  if temp.exists():temp.unlink()
if digest(a)!=SHA:raise ValueError('Cached archive checksum mismatch')
with tarfile.open(a) as t:
 for member,name in [('1.1/data/ru-RU.jsonl','ru-RU.jsonl'),('1.1/LICENSE','LICENSE'),('1.1/NOTICE.md','NOTICE.md')]:
  m=t.getmember(member)
  if not m.isfile() or m.size>20*1024*1024:raise ValueError('Unexpected archive entry')
  (ROOT/name).write_bytes(t.extractfile(m).read())
(ROOT/'source.json').write_text(json.dumps({'url':URL,'archive_sha256':SHA,'locale':'ru-RU','license':'CC BY 4.0','source':'https://github.com/alexa/massive'},indent=2)+'\n')
print('MASSIVE 1.1 ru-RU ready; checksum verified.')
