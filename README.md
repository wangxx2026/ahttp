### 发起一步http请求

####依赖libevent库

####demo

```php
$obj = new ahttp();
$obj->set_time_out(1000);
$obj->get('http://www.google.com');
$obj->post('http://example.com', array('data' => 'aaaaaa'));
$obj->get('http://example.com', array('header' => array('User-agent' => 'ahttp')));
$obj->wait_reply();
$arr = $obj->result();
```

返回值的顺序按发起的返回
