"""기존 픽셀 원화의 조명 비교 화면을 대화용으로 묶습니다."""
import argparse
import base64
import json
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scene',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    source=Path(__file__).resolve().parent
    scene=json.loads((args.scene/'scene.json').read_text())
    scene['images']={name:'data:image/png;base64,'+base64.b64encode((args.scene/(name+'.png')).read_bytes()).decode('ascii') for name in ['original','albedo','normals','surface','material','owners']}
    text=(source/'pixel_preview.html').read_text(encoding='utf-8')
    text=text.replace('__PIXEL_DATA__',json.dumps(scene,separators=(',',':')))
    text=text.replace('__PIXEL_WORKER__',(source/'pixel_worker.js').read_text(encoding='utf-8'))
    if len(text.encode())>=1000000:raise ValueError('대화 화면은 1MB보다 작아야 합니다.')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(text,encoding='utf-8')
    print(args.output,len(text.encode()),'bytes')

if __name__=='__main__':main()
