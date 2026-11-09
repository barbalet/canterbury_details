"""Offline importer contracts; no mutation of either real submodule."""
import copy,json,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import compile_content as c

def small_snapshot(d):
    # Keep real record shapes/provenance but a small consistent reference graph.
    out={k:{a:v for a,v in doc.items() if a not in ('features','objects','network','nodes','edges','sites','candidates','elements','routes','ways','turn_restrictions')} for k,doc in d.items()}
    feature_ids=set()
    for key in ('buildings','road_ink','waterways','rail_ink'):
        if key=='buildings':obj=d[key]['objects'][0]
        else:
            edge=d[key]['network']['edges'][0];obj=next(o for o in d[key]['objects'] if o['id']==edge['object_id'])
            out[key]['network']={'nodes':[n for n in d[key]['network']['nodes'] if n['id'] in (edge['from'],edge['to'])],'edges':[edge]}
        out[key]['objects']=[obj];feature_ids.update(obj['source_feature_ids'])
    out['vectors']['features']=[f for f in d['vectors']['features'] if f['id'] in feature_ids]
    bid=out['buildings']['objects'][0]['id']
    for key in ('buildings_modern','buildings_1940'):out[key]['objects']=[next(o for o in d[key]['objects'] if o['id']==bid)]
    for key in ('historical_roads','modern_roads','rail_corridors'):
        graph=d[key].get('network',d[key]);edge=graph['edges'][0];nodes=[n for n in graph['nodes'] if n['id'] in (edge['from'],edge['to'])]
        if key=='rail_corridors':out[key].update(nodes=nodes,edges=[edge])
        else:out[key]['network']={'nodes':nodes,'edges':[edge]}
        if key=='historical_roads':out[key]['routes']=[r for r in d[key]['routes'] if r['id'] in edge['route_ids']]
    site=next(s for s in d['institutions']['sites'] if s.get('position_pixels') is None)
    out['institutions']['sites']=[site];out['institution_policy']['sites']=[s for s in d['institution_policy']['sites'] if s['id']==site['id']]
    out['review_candidates']['candidates']=[]
    refs={p['osm_ref'] for p in d['controls']['points']}
    match=out['buildings_modern']['objects'][0]['modern_match']
    if 'osm_ref' in match:refs.add(match['osm_ref'])
    refs.update(p['osm_ref'] for p in out['buildings_modern']['objects'][0]['modern_candidates'])
    out['osm']['elements']=[o for o in d['osm']['elements'] if f"{o['type']}/{o['id']}" in refs]
    way_id=out['modern_roads']['network']['edges'][0]['osm_way_id'];way=next(w for w in d['osm_roads']['ways'] if w['id']==way_id)
    out['osm_roads']['ways']=[way];out['osm_roads']['nodes']=[n for n in d['osm_roads']['nodes'] if n['id'] in way['node_ids']]
    return out

class ContentContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        d,cls.inputs,cls.owned,cls.manifest=c.read_inputs();cls.base=small_snapshot(d)
        cls.policy=c.parse(cls.owned['content/content-policy.json']);cls.review=c.parse(cls.owned['content/review-annotations.json'])
        c.validate(cls.base,cls.inputs,cls.policy,cls.review)
    def setUp(self):self.d=copy.deepcopy(self.base);self.review=copy.deepcopy(self.__class__.review)
    def validate(self):return c.validate(self.d,self.inputs,self.policy,self.review)
    def reject(self,mutate):
        mutate()
        with self.assertRaises(c.ContentError):self.validate()
    def test_consistent_reference_fixture(self):self.validate()
    def test_duplicate_candidate(self):self.reject(lambda:self.d['buildings']['objects'].append(self.d['buildings']['objects'][0]))
    def test_duplicate_source_feature(self):self.reject(lambda:self.d['vectors']['features'].append(self.d['vectors']['features'][0]))
    def test_missing_source_reference(self):self.reject(lambda:self.d['buildings']['objects'][0].update(source_feature_ids=[9223372036854775807]))
    def test_dimension_mismatch(self):self.reject(lambda:self.d['waterways'].update(width=1))
    def test_open_ring(self):self.reject(lambda:self.d['buildings']['objects'][0]['geometry']['rings'][0].pop())
    def test_out_of_canvas(self):self.reject(lambda:self.d['vectors']['features'][0]['rings'][0][0].__setitem__(0,-1))
    def test_dangling_edge(self):self.reject(lambda:self.d['modern_roads']['network']['edges'][0].update(to='missing'))
    def test_network_flag(self):self.reject(lambda:self.d['modern_roads']['network']['edges'][0].update(traversable=True))
    def test_non_boolean_flag(self):self.reject(lambda:self.d['road_ink']['objects'][0].update(network_included=1))
    def test_enrichment_cannot_change_footprint(self):self.reject(lambda:self.d['buildings_modern']['objects'][0].update(area_pixels=999))
    def test_unlocated_site_stays_unlocated(self):self.reject(lambda:self.d['institutions']['sites'][0].update(candidate_buildings=[{'id':'missing','identity_verified':False}]))
    def test_stale_upstream_lineage(self):self.reject(lambda:self.d['modern_roads'].update(generator_sha256='0'*64))
    def test_license_required(self):self.reject(lambda:self.d['modern_roads'].update(license='MIT'))
    def test_unsupported_version(self):self.reject(lambda:self.d['modern_roads'].update(version=99))
    def test_duplicate_osm_identity(self):self.reject(lambda:self.d['osm']['elements'].append(self.d['osm']['elements'][0]))
    def test_stale_project_review(self):self.review['vectors_sha256']='0'*64;self.assertRaises(c.ContentError,self.validate)
    def test_review_is_independent_and_never_auto_approves(self):
        record=self.d['buildings']['objects'][0]
        note={'dataset':'buildings','record_id':record['id'],'source_sha256':self.inputs['buildings']['sha256'],'decision':'retain_candidate','note':'Technical fixture review; no historical assertion'}
        self.review['annotations']=[note];self.validate();self.assertEqual(record['status'],'unreviewed')
        self.review['annotations'].append(note);self.assertRaises(c.ContentError,self.validate)
    def test_stale_review_target(self):
        self.review['annotations']=[{'dataset':'buildings','record_id':self.d['buildings']['objects'][0]['id'],'source_sha256':'0'*64,'decision':'needs_review','note':'fixture'}]
        self.assertRaises(c.ContentError,self.validate)
    def test_parser_rejects_invalid_numbers_and_duplicate_keys(self):
        for raw in (b'{"x":NaN}',b'{"x":Infinity}',b'{"x":1e999}',b'{"x":1,"x":2}',b'{"x":9223372036854775808}'):
            with self.subTest(raw=raw):self.assertRaises(c.ContentError,c.parse,raw)
    def test_endpoint_and_length_validation(self):
        graph=copy.deepcopy(self.d['modern_roads']['network']);graph['edges'][0]['length_pixels']+=3
        self.assertRaises(c.ContentError,c.network,graph,1600,1600)
    def test_schema_rejects_missing_extra_and_wrong_type(self):
        schema={'type':'object','required':['version'],'properties':{'version':{'type':'integer'}},'additionalProperties':False}
        for value in ({},{'version':True},{'version':1,'extra':2}):self.assertRaises(c.ContentError,c.schema_check,value,schema)
    def test_no_external_input_or_output_paths(self):
        self.assertRaises(c.ContentError,c.local_file,c.ROOT,'../nosferatu-era/README.md')
        self.assertRaises(c.ContentError,c.output_path,Path('/private/tmp/outside.json'))
        with tempfile.TemporaryDirectory() as name:
            root=Path(name)/'root';root.mkdir();outside=Path(name)/'outside';outside.write_text('must not read');(root/'link').symlink_to(outside)
            self.assertRaises(c.ContentError,c.local_file,root,'link')
    def test_rejected_structural_input_preserves_generation(self):
        with tempfile.TemporaryDirectory() as name:
            path=Path(name)/'generation.json';path.write_bytes(b'previous valid generation')
            self.d['modern_roads']['network']['edges'][0]['traversable']=True
            with patch.object(c,'read_inputs',return_value=(self.d,self.inputs,self.owned,self.manifest)):
                self.assertRaises(c.ContentError,c.compile_to,path)
            self.assertEqual(path.read_bytes(),b'previous valid generation');self.assertEqual(len(list(path.parent.iterdir())),1)
    def test_stale_physical_input_preserves_generation(self):
        with tempfile.TemporaryDirectory() as name:
            root=Path(name)
            for file,raw in self.owned.items():
                target_file=root/file;target_file.parent.mkdir(parents=True,exist_ok=True);target_file.write_bytes(raw)
            (root/'canterbury/newcanterbury').mkdir(parents=True);(root/'apesdk').mkdir();(root/'LICENSE').write_bytes((c.ROOT/'LICENSE').read_bytes())
            for source in c.FILES.values():
                file=root/'canterbury'/source;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(b'{}')
            target=root/'generation.json';target.write_bytes(b'previous')
            with patch.object(c,'ROOT',root),patch.object(c,'inventory',return_value=self.manifest):self.assertRaises(c.ContentError,c.compile_to,target)
            self.assertEqual(target.read_bytes(),b'previous')
    def test_write_and_rename_failures_preserve_generation(self):
        for operation in ('fsync','replace'):
            with tempfile.TemporaryDirectory() as name:
                path=Path(name)/'generation.json';path.write_bytes(b'previous')
                with patch.object(c.os,operation,side_effect=OSError('injected filesystem failure')):self.assertRaises(OSError,c.atomic_write,path,b'new')
                self.assertEqual(path.read_bytes(),b'previous');self.assertEqual(list(path.parent.iterdir()),[path])
    def test_captured_owned_snapshot_must_match_initial_lock(self):
        changed=dict(self.owned);changed['content/review-annotations.json']=b'changed after initial capture'
        with patch.object(c,'read_inputs',return_value=(self.d,self.inputs,changed,self.manifest)):
            self.assertRaisesRegex(c.ContentError,'initial compilation lock',c.compile_bytes,{p:c.digest(raw) for p,raw in self.owned.items()},self.manifest)
    def test_changed_inputs_before_commit_preserve_generation(self):
        with tempfile.TemporaryDirectory() as name:
            path=Path(name)/'generation.json';path.write_bytes(b'previous')
            def stale():raise c.ContentError('injected input change before commit')
            self.assertRaises(c.ContentError,c.atomic_write,path,b'new',stale)
            self.assertEqual(path.read_bytes(),b'previous');self.assertEqual(list(path.parent.iterdir()),[path])
    def test_dangling_review_reference(self):
        edge=self.d['road_ink']['network']['edges'][0]
        self.d['review_candidates']['candidates']=[{'id':'bad-review','kind':'reference_rail_crossing','layer':'roads','edge_id':edge['id'],'corridor_id':'missing','status':'unreviewed'}]
        self.assertRaises(c.ContentError,self.validate)
    def test_atomic_success(self):
        with tempfile.TemporaryDirectory() as name:
            path=Path(name)/'generation.json';c.atomic_write(path,b'new');self.assertEqual(path.read_bytes(),b'new')

if __name__=='__main__':unittest.main()
